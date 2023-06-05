
vec3 Reinhard(vec3 color) {
	return color / (color + vec3(1.0));
}

vec3 Reinhard1(vec3 color, float maxLuminosity) {
	return color * (1 + color / pow(maxLuminosity, 2)) / (color + vec3(1.0));
}

vec3 ACESFilm(vec3 x)
{
	/* From https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/ */
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;

	return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0, 1);
}

#include "aces_hill.glsl"

