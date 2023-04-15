#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform SceneData {
    vec4 fogColor;
    vec4 fogDistances;
    vec4 ambientColor;
    vec4 sunlightDir;
    vec4 sunlightColor;
} sceneData;

layout(set = 1, binding = 0) uniform sampler2D tex1;

void main() {
    outColor = vec4(texture(tex1,texCoord));
}