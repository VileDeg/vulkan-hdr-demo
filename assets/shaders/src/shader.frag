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
    float shadowOpacity; // Unused?
    bool showShadowMap;

    bool enablePCF;
    float shadowMapDisplayBrightness;
    int _pad2;
    int _pad3;

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

// array of offset direction for sampling
const vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);



float shadowCalculation(samplerCube shadowCubeMap, vec3 fragPos, vec3 lightPos, vec3 cameraPos, float farPlane, float shadowBias, bool enablePCF)
{
    // Sample shadow cube map
    vec3 lightToFrag = fragPos - lightPos;

	// Check if fragment is in shadow
    float currentDepth = length(lightToFrag);

    float shadow = 0.0;
    
    if (enablePCF) {
        const int PCF_samples = 20;

        float viewDistance = length(cameraPos - fragPos);
        float diskRadius = (1.0 + (viewDistance / farPlane)) / 25.0;
        for(int i = 0; i < PCF_samples; ++i)
        {
            float sampledDepth = texture(shadowCubeMap, lightToFrag + gridSamplingDisk[i] * diskRadius).r;
            //sampledDepth *= farPlane;   // undo mapping [0;1]
            if(currentDepth - shadowBias > sampledDepth) {
                shadow += 1.0;
            }
        }
        shadow /= float(PCF_samples);
    } else {
        float sampledDepth = texture(shadowCubeMap, lightToFrag).r;
        //sampledDepth *= farPlane;
        if(currentDepth - shadowBias > sampledDepth) {
            shadow = 1.0;
        }
    }

    return shadow;
}

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
            if (sd.showShadowMap) {
                vec3 lightToFrag = fragPos - sd.lights[0].pos;
                float sampledDepth = texture(shadowCubeMap, lightToFrag).r;
                sampledDepth /= sd.lightFarPlane;
                FragColor = vec4(vec3(sampledDepth), 1.0) * sd.shadowMapDisplayBrightness;
                return;
            }

            // Calculate total lighting from all sources
            vec3 lightVal = calculateLighting(sd.lights, fragPos, normal, sd.cameraPos);
           
            // Adjust light intensity for shadow
            float shadow = shadowCalculation(shadowCubeMap, fragPos, sd.lights[0].pos, sd.cameraPos, sd.lightFarPlane, sd.shadowBias, sd.enablePCF);

            // Apply lighting
            result = result * (sd.ambientColor + lightVal * (1.0 - shadow));
        }

        if (ssbo.enableExposure == 1) {
            result *= pow(2, ssbo.exposure);
        }
    }
   
    FragColor = vec4(result, 1.f);
}