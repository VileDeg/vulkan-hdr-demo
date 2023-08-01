#version 460

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 uv;
//layout(location = 3) in flat vec4 objectColor;
layout(location = 3) in vec3 normal;
layout(location = 4) in vec3 uvw; //For skybox cubemap sampling


layout(location = 0) out vec4 FragColor;

#include "incl/defs.glsl"
#include "incl/light.glsl"
#include "incl/bump_mapping.glsl" 

layout(push_constant) uniform ScenePC {
    bool lightAffected;
    bool isCubemap;
    uint objectIndex;
    uint meshIndex;

	bool useDiffTex;
	bool useBumpTex;
} pc;

layout(set = 0, binding = 1)
#include "incl/sceneUB.incl" 
sd;

layout(std430, set = 0, binding = 2)
#include "incl/objectSSBO.incl" 
ssbo;

layout(set = 0, binding = 3) uniform samplerCube skybox;
layout(set = 0, binding = 4) uniform samplerCubeArray shadowCubeArray;

// Supplied from push descriptor set
layout(set = 1, binding = 0) uniform sampler2D diffuse;
layout(set = 1, binding = 1) uniform sampler2D bump;

void main()  
{
    vec3 bumpNormal = normal;
    vec2 bumpUV = uv;

    if (sd.enableBumpMapping && !pc.isCubemap && pc.useBumpTex) {
        bumpMapping(bump, sd.bumpStep, mat3(ssbo.objects[pc.objectIndex].normalMatrix), sd.bumpStrength, sd.bumpUVFactor, bumpNormal, bumpUV);
    }

    if (sd.showNormals) {
        FragColor = vec4(bumpNormal, 1.f);
        return;
    }

    vec3 result = vec3(0);

    if (pc.isCubemap) {
        result = texture(skybox, uvw).rgb;
    } else {
        if (pc.useDiffTex) {
            result = texture(diffuse, bumpUV).rgb; 
        } else {
            //result = objectColor.rgb;
            result = ssbo.objects[pc.objectIndex].mat[pc.meshIndex].diffuseColor;
        }
    }

    if (pc.lightAffected) {

        if (!pc.isCubemap) {
            if (sd.showShadowMap) {
                vec3 lightToFrag = fragPos - sd.lights[sd.shadowMapDisplayIndex].pos;
                float sampledDepth = texture(shadowCubeArray, vec4(lightToFrag, sd.shadowMapDisplayIndex)).r;
                sampledDepth /= sd.lightFarPlane;
                FragColor = vec4(vec3(sampledDepth), 1.0) * sd.shadowMapDisplayBrightness;
                return;
            }

            // Apply lighting
            result *= calculateLighting(sd.lights, ssbo.objects[pc.objectIndex].mat[pc.meshIndex], 
                sd.ambientColor, fragPos, bumpNormal, sd.cameraPos, 
                sd.enableShadows, shadowCubeArray, sd.lightFarPlane, sd.shadowBias, sd.enablePCF);
        }

        if (sd.enableExposure) {
            result *= pow(2, sd.exposure);
        }
    }
   
    FragColor = vec4(result, 1.f);
}