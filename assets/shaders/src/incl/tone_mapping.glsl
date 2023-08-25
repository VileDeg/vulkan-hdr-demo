#include "defs.glsl"

vec3 Reinhard(vec3 color) {
	return color / (color + vec3(1.0));
}

float sRGBtoLin(float colorChannel) {
	// From: https://stackoverflow.com/questions/596216/formula-to-determine-perceived-brightness-of-rgb-color
	// Send this function a decimal sRGB gamma encoded color value
	// between 0.0 and 1.0, and it returns a linearized value.

	if (colorChannel <= 0.04045) {
		return colorChannel / 12.92;
	} else {
		return pow(((colorChannel + 0.055)/1.055),2.4);
	}
}

float luminance(vec3 rgb) {
    return dot(rgb, RGB_TO_LUM);
}

vec3 ReinhardExtended(vec3 v)  { //, float max_white_lum
    vec3 numerator = v * (1.0 + v / (WHITE_POINT * WHITE_POINT));
    return numerator / (1.0 + v);
}

vec3 Uncharted2TonemapPartial(vec3 x) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 Uncharted2Filmic(vec3 v) {
    float exposure_bias = 2.0;
    vec3 curr = Uncharted2TonemapPartial(v * exposure_bias);

    vec3 W = vec3(11.2);
    vec3 white_scale = vec3(1.0) / Uncharted2TonemapPartial(W);
    return curr * white_scale;
}

vec3 ACESFilm(vec3 x) {
	/* From https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/ */
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;

	return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0, 1);
}


// Based on https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl

// sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
const mat3 ACESInputMat = mat3(
    0.59719, 0.35458, 0.04823,
    0.07600, 0.90834, 0.01566,
    0.02840, 0.13383, 0.83777
);

// ODT_SAT => XYZ => D60_2_D65 => sRGB
const mat3 ACESOutputMat = mat3(
     1.60475, -0.53108, -0.07367,
    -0.10208,  1.10813, -0.00605,
    -0.00327, -0.07276,  1.07602
);

vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 ACESFitted(vec3 color)
{
    color = color * ACESInputMat;

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    color = color * ACESOutputMat;

    // Clamp to [0, 1]
    color = clamp(color, 0, 1);

    return color;
}

float howCloseToHalf(float val) {
    float sigma = 0.2;
    return exp(-pow(val-0.5, 2) / 2*pow(sigma,2));
}

float RGBHowCloseToHalf(vec3 rgb) {
    return howCloseToHalf(rgb.r) * howCloseToHalf(rgb.g) * howCloseToHalf(rgb.b);
}


vec3 applyGlobalToneMapping(vec3 col, int mode) {
	vec3 outCol = col;
	switch (mode) {
        case 0: outCol = ReinhardExtended(col); break;
        case 1: outCol = Reinhard(col); break;
        case 2: outCol = Uncharted2Filmic(col); break;
        case 3: outCol = ACESFilm(col); break;
        case 4: outCol = ACESFitted(col); break;
    }
	return outCol;
}