#version 460

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in flat vec4 objectColor;
layout(location = 4) in vec3 normal;
layout(location = 5) in vec3 uvw; //For skybox cubemap sampling
//layout(location = 6) in vec3 worldPos; //Fragment world pos for shadow cubemap sampling

layout(location = 0) out vec4 FragColor;

#include "incl/defs.glsl"
#include "incl/light.glsl"

layout(push_constant) uniform PushConstants {
    int hasTextures;
    int lightAffected;
    int isCubemap;
    int _pad0;
} pc;

layout(set = 0, binding = 1)
#include "incl/sceneUB.incl" 
sd;

layout(std430, set = 1, binding = 0)
#include "incl/objectSSBO.incl" 
ssbo;

layout(set = 1, binding = 1) uniform samplerCube skybox;
layout(set = 1, binding = 2) uniform samplerCubeArray shadowCubeArray;

layout(set = 2, binding = 0) uniform sampler2D diffuse;

/*layout(set = 2, binding = 1) uniform Material {
    vec3 ambientColor;
    vec3 diffuseColor;
    vec3 specularColor; 
} mat;*/

void main()  
{
    if (sd.showNormals) {
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
                vec3 lightToFrag = fragPos - sd.lights[sd.shadowMapDisplayIndex].pos;
                float sampledDepth = texture(shadowCubeArray, vec4(lightToFrag, sd.shadowMapDisplayIndex)).r;
                sampledDepth /= sd.lightFarPlane;
                FragColor = vec4(vec3(sampledDepth), 1.0) * sd.shadowMapDisplayBrightness;
                return;
            }

            // Apply lighting
            result *= calculateLighting(sd.lights, sd.ambientColor, fragPos, normal, sd.cameraPos,
                sd.enableShadows, shadowCubeArray, sd.lightFarPlane, sd.shadowBias, sd.enablePCF);
        }

        if (sd.enableExposure) {
            result *= pow(2, sd.exposure);
        }
    }
   
    FragColor = vec4(result, 1.f);
}