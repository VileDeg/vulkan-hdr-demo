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

struct ObjectData{
	mat4 model;
	vec4 color;

    int useObjectColor;
    int _pad0;
    int _pad1;
    int _pad2;
};

layout(std140, set = 1, binding = 0) buffer GlobalBuffer{
    uint newMax;
    uint oldMax;
    int showNormals;
    float exposure;

    int exposureON;
    int exposureMode;
    int toneMappingON;
    int toneMappingMode;

	ObjectData objects[MAX_OBJECTS];
} ssbo;

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

const float keyValue = 1.; //middle gray

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

        float eps = 0.001;
        float lum = luminance(result);

    
        // Update current max only if current luminance is bigger than 50% of old max
        if (lum > 0.5*uintBitsToFloat(ssbo.oldMax)) {
            // If difference is not too small, upgrade new max
            if (lum - uintBitsToFloat(ssbo.newMax) > eps) {
                atomicMax(ssbo.newMax, floatBitsToUint(lum));
            }
        }
        

        if (ssbo.exposureON == 1) {
            /*if (f_oldMax > 1.f) { // TODO: remove condition
                switch (ssbo.exposureMode) {
                case 0: result = result / log(f_oldMax); break;
                case 1: result = result / f_oldMax; break;
                }
            }*/
            result *= keyValue / (f_oldMax - ssbo.exposure);
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