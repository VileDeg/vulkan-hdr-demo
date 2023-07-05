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

    if (pc.lightAffected == 1) {

        if (pc.isCubemap == 0) {
            result *= calculateLighting(sd.lights, sd.ambientColor, fragPos, normal, sd.cameraPos);
        }
        
        if (ssbo.enableExposure == 1) {
            result *= pow(2, ssbo.exposure);
        }
    }
   
    FragColor = vec4(result, 1.f);
}