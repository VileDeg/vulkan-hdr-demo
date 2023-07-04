#version 460

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in flat vec4 objectColor;
layout(location = 4) in vec3 normal;

layout(location = 5) in vec3 uvw; //For cubemap sampling

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
    int isCubemap;
    int _pad0;
} pc;

layout(set = 2, binding = 0) uniform sampler2D diffuse;
layout(set = 1, binding = 1) uniform samplerCube skybox;

const float eps = 0.001;
const float lumMaxTreshold = 0.5;

const float expTerm = 9.6; // Simplified term. From: https://bruop.github.io/exposure/

const float expMin = 1e2;
const float expMax = 1e-4;


void main()  
{
    if (ssbo.showNormals == 1) {
        FragColor = vec4(normal, 1.f);
        return;
    }

    vec3 result = vec3(0);

    if (pc.isCubemap == 1) {
        result = texture(skybox, uvw).rgb;
    } else {
        if (pc.hasTextures == 1) {
            result = texture(diffuse, texCoord).rgb; 
        } else {
            result = objectColor.rgb;

        }
    }
   

    //float f_oldMax = uintBitsToFloat(ssbo.oldMax);

    if (pc.lightAffected == 1) {

        if (pc.isCubemap == 0) {
            result *= calculateLighting(sd.lights, sd.ambientColor, fragPos, normal, sd.cameraPos);
        }
        
        /*float lum = luminance(result);

        // Update current max
        if (lum > uintBitsToFloat(ssbo.newMax)) {
                atomicMax(ssbo.newMax, floatBitsToUint(lum));
        }*/

        // Update luminance histogram
        //int bin = int(lum / (f_oldMax + 0.0001) * MAX_BINS);
        //uint bin = colorToBin(result, sd.minLogLum, sd.oneOverLogLumRange);
        //atomicAdd(ssbo.luminance[bin], 1);
        

        /*if (ssbo.exposureON == 1) {
            float expa = ssbo.exposureAverage; //  + 0.0001f
            //result = result / (expMin + (expa / (expa + 1)) * expMax);
            result = result / (9.6f * expa);
        } */

        result *= pow(2, ssbo.exposure);
    }
   
    /*if (ssbo.toneMappingON == 1) { // If enable tone mapping
        switch (ssbo.toneMappingMode) {
        case 0: result = ReinhardExtended(result, 10.f); break;
        case 1: result = Reinhard(result); break;
        case 2: result = Uncharted2Filmic(result); break;
        case 3: result = ACESFilm(result); break;
        case 4: result = ACESFitted(result); break;
        }
    }*/

   
    FragColor = vec4(result, 1.f);
}