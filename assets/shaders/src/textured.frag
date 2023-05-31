#version 460

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in flat vec4 objectColor;
layout(location = 4) in vec3 normal;

layout(location = 0) out vec4 FragColor;

#include "light.glsl"
#include "tone_mapping.glsl"

layout(set = 0, binding = 1) uniform SceneData {
    vec4 cameraPos;
    vec4 ambientColor;
    LightData[MAX_LIGHTS] lights;
} sd;

layout(set = 2, binding = 0) uniform sampler2D tex1;

layout(push_constant) uniform constants {
	ivec4 data; //ivec4
	mat4 render_matrix;
} pc;

void main() {
    vec3 lightVal = 
        calculateLighting(sd.lights, sd.ambientColor, fragPos, normal, sd.cameraPos.xyz);

    vec4 tex = texture(tex1, texCoord); 
    vec3 result = lightVal * tex.rgb;

    if (pc.data.x == 1) { // If enable tone mapping
        switch (pc.data.y) {
        case 0: result = Reinhard(result)  ; break;
        case 1: result = ACESFilm(result)  ; break;
        case 2: result = ACESFitted(result);
        }
    }

    FragColor = vec4(result, tex.a);
}