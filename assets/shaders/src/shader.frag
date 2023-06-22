#version 460

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in flat vec4 objectColor;
layout(location = 4) in vec3 normal;
//layout(location = 5) in flat int objectIndex;

layout(location = 0) out vec4 FragColor;

#include "defs.glsl"
#include "light.glsl"
#include "tone_mapping.glsl"

#include "structs.glsl"

layout(set = 0, binding = 1) uniform SceneData {
    vec3 cameraPos;
    int _pad0;

    vec3 ambientColor;
    int _pad1;

    LightData[MAX_LIGHTS] lights;
} sd;

layout(push_constant) uniform PushConstants {
    int hasTextures;
    int lightAffected;
    int _pad0;
    int _pad1;
} pc;

layout(set = 2, binding = 0) uniform sampler2D diffuse;

//const float keyValue = 0.18; // middle gray
const float eps = 0.001;
const float lumMaxTreshold = 0.5;

const float expTerm = 9.6; // Simplified term. From: https://bruop.github.io/exposure/

void main() 
{
    if (ssbo.showNormals == 1) {
        FragColor = vec4(normal, 1.f);
        return;
    }

    vec3 result = vec3(0);

    if (pc.hasTextures == 1) {
        result = texture(diffuse, texCoord).rgb; 
    } else {
        result = objectColor.rgb;
    }

    float f_oldMax = uintBitsToFloat(ssbo.oldMax);

    if (pc.lightAffected == 1) {
        result *= calculateLighting(sd.lights, sd.ambientColor, fragPos, normal, sd.cameraPos);

        // Apply exposure
        //result *= ssbo.exposure;

        
        float lum = luminance(result);

        // Update current max only if current luminance is bigger than 50% of old max
        
        if (lum > lumMaxTreshold * uintBitsToFloat(ssbo.oldMax)) {
            // If difference is not too small, upgrade new max
            if (lum - uintBitsToFloat(ssbo.newMax) > eps) {
                atomicMax(ssbo.newMax, floatBitsToUint(lum));
            }
        }
        
        // Update luminance histogram
        int bin = int(lum / f_oldMax * MAX_BINS);
        atomicAdd(ssbo.luminance[bin], 1); //.val

        if (ssbo.exposureON == 1) {
            /*if (f_oldMax > 1.f) { // TODO: remove condition
                switch (ssbo.exposureMode) {
                case 0: result = result / log(f_oldMax); break;
                case 1: result = result / f_oldMax; break;
                }
            }*/
            //float commonLum = ssbo.commonLumBin / MAX_BINS * f_oldMax;

            //float denom = expTerm * (ssbo.commonLuminance + 0.0001);
            //if (denom - 1.f > eps) {
            result *= 1 / ssbo.exposureAverage;
            result *= pow(2, ssbo.exposure);
            //}
        } 
    }
   
    if (ssbo.toneMappingON == 1) { // If enable tone mapping
        switch (ssbo.toneMappingMode) {
        case 0: result = ReinhardExtended(result, f_oldMax); break;
        case 1: result = Reinhard(result); break;
        case 2: result = Uncharted2Filmic(result); break;
        case 3: result = ACESFilm(result); break;
        case 4: result = ACESFitted(result); break;
        }
    }

    /*if (ssbo.exposureON == 0) {
        vec3 evPlus  = result * 2;
        vec3 evMinus = result / 2;

        float weight = RGBHowCloseToHalf(result);

        if (lum < 0.5) {
            result = result * weight + evPlus * (1-weight);
        } else {
            result = result * weight + evMinus * (1-weight);
        }
    }*/

    FragColor = vec4(result, 1.f);
}