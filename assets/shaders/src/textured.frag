#version 460
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec4 objectColor;
layout(location = 4) in vec3 normal;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform SceneData {
    vec4 ambientColor;
    vec4 lightPos;
    vec4 cameraPos;
} sd;

layout(set = 2, binding = 0) uniform sampler2D tex1;

#include "light.glsl"

void main() {
    vec3 lightColor = sd.ambientColor.xyz;
    vec3 lightPos = sd.lightPos.xyz;

    vec3 lightVal = calcLighting(lightColor, lightPos, fragPos, normal, sd.cameraPos.xyz);

    vec4 tex = texture(tex1, texCoord); 
    vec3 result = lightVal * tex.rgb;

    FragColor = vec4(result, tex.a);
}