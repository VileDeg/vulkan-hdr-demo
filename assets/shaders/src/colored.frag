#version 460

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in flat vec4 objectColor;
layout(location = 4) in vec3 normal;

layout(location = 0) out vec4 FragColor;

#include "light.glsl"

layout(set = 0, binding = 1) uniform SceneData {
    vec4 cameraPos;
    vec4 ambientColor;
    LightData[MAX_LIGHTS] lights;
} sd;

layout(push_constant) uniform constants {
	ivec4 data;
	mat4 render_matrix;
} pc;

void main() {
    /*vec3 lightVal = 
        calculateLighting(sd.lights, sd.ambientColor, fragPos, normal, sd.cameraPos.xyz);*/
    vec3 result = objectColor.xyz;
    //if (pc.data.x == 1) { // If enable HDR
    //    result = reinhart(result);
    //}
    //result = reinhart(result);

    FragColor = vec4(result, 1.0);
}