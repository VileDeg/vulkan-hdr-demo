#version 460

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in flat vec4 objectColor;
layout(location = 4) in vec3 normal;
layout(location = 5) in vec3 uvw; //For skybox cubemap sampling
//layout(location = 6) in vec3 worldPos; //Fragment world pos for shadow cubemap sampling

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

    float lightFarPlane;
    float shadowBias;
    float shadowOpacity;
    bool showShadowMap;

    float shadowMapDisplayBrightness;
    int _pad2;
    int _pad3;
    int _pad4;

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

layout(set = 1, binding = 2) uniform samplerCube shadowCubeMap;

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
            // Calculate total lighting from all sources
            vec3 lightVal = calculateLighting(sd.lights, fragPos, normal, sd.cameraPos);
            // Sample shadow cube map
            vec3 lightVec = fragPos - sd.lights[0].pos;
            float sampledDist = texture(shadowCubeMap, lightVec).r;

	        // Check if fragment is in shadow
            float dist = length(lightVec);
            float shadow = (dist <= sampledDist * sd.lightFarPlane + sd.shadowBias) ? 1.0 : sd.shadowOpacity;
            // Adjust light intensity for shadow
            lightVal *= shadow;

            // Apply lighting
            result *= (sd.ambientColor + lightVal);

            if (sd.showShadowMap) {
                FragColor = vec4(vec3(sampledDist), 1.0) * sd.shadowMapDisplayBrightness;
                return;
            }
        }

        if (ssbo.enableExposure == 1) {
            result *= pow(2, ssbo.exposure);
        }
    }
   
    FragColor = vec4(result, 1.f);
}