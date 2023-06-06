#version 460
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in flat vec4 objectColor;
layout(location = 4) in vec3 normal;

layout(location = 0) out vec4 FragColor;

#include "defs.glsl"
#include "light.glsl"
#include "tone_mapping.glsl"


struct ObjectData{
	mat4 model;
	vec4 color;
};

layout(std140, set = 1, binding = 0) buffer GlobalBuffer{
    int exposureON;
    int toneMappingON;
    int toneMappingMode;
    float exposure;

    uint newMax;
    uint oldMax;
	int  _pad0;
    int  _pad1;

	ObjectData objects[MAX_OBJECTS];
} ssbo;

layout(set = 0, binding = 1) uniform SceneData {
    vec3 cameraPos;
    int _pad0;

    vec3 ambientColor;
    int _pad1;

    LightData[MAX_LIGHTS] lights;
} sd;

layout(set = 2, binding = 0) uniform sampler2D tex1;

void main() 
{
    vec3 lightVal = calculateLighting(sd.lights, sd.ambientColor, fragPos, normal, sd.cameraPos);

    vec4 tex    = texture(tex1, texCoord); 
    vec3 result = lightVal * tex.rgb;

    if (ssbo.exposureON == 1) { // If enable exposure
        float eps = 0.001;
         // If difference is not too small, update maximum value
        float sum = dot(vec3(1), result);
        if (abs(sum - uintBitsToFloat(ssbo.newMax)) > eps) {
            atomicMax(ssbo.newMax, floatBitsToUint(sum));
        }
    
        float f_oldMax = uintBitsToFloat(ssbo.oldMax);
        if (f_oldMax > eps) {
            result = result / f_oldMax * ssbo.exposure;
        }
        //result *= ssbo.exposure;
    }
   
    if (ssbo.toneMappingON == 1) { // If enable tone mapping
        switch (ssbo.toneMappingMode) {
        case 0: result = Reinhard(result)  ; break;
        case 1: result = ACESFilm(result)  ; break;
        case 2: result = ACESFitted(result);
        }
    }

    FragColor = vec4(result, tex.a);
}