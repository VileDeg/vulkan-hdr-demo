#version 460

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
    int exposureMode;
    int toneMappingON;
    int toneMappingMode;

    uint newMax;
    uint oldMax;
    float exposure;
	int  _pad0;

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

    // Apply exposure
    result *= ssbo.exposure;

    float eps = 0.001;
    float lum = luminance(result);

    if (ssbo.exposureON == 1) {
        // Update current max only if current luminance is bigger than 90% of old max
        if (lum > 0.5*uintBitsToFloat(ssbo.oldMax)) {
            // If difference is not too small, upgrade new max
            if (lum - uintBitsToFloat(ssbo.newMax) > eps) {
                atomicMax(ssbo.newMax, floatBitsToUint(lum));
            }
        }
    
        float f_oldMax = uintBitsToFloat(ssbo.oldMax);    
        if (f_oldMax > 1.f) { // TODO: remove condition
            switch (ssbo.exposureMode) {
            case 0: result = result / log(f_oldMax); break;
            case 1: result = result / f_oldMax; break;
            }
        }
    }
   
    if (ssbo.toneMappingON == 1) { // If enable tone mapping
        switch (ssbo.toneMappingMode) {
        case 0: result = Reinhard(result); break; //ReinhardExtended(result, f_oldMax); break;
        case 1: result = Reinhard(result); break;
        case 2: result = Uncharted2Filmic(result); break;
        case 3: result = ACESFilm(result); break;
        case 4: result = ACESFitted(result); break;
        }
    }

    FragColor = vec4(result, tex.a);
}