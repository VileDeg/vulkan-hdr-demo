#version 460
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in flat vec4 objectColor;
layout(location = 4) in vec3 normal;

layout(location = 0) out vec4 FragColor;

//in vec4 gl_FragCoord;



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


struct ObjectData{
	mat4 model;
	vec4 color;
};

layout(std140, set = 1, binding = 0) buffer ObjectBuffer{
	uvec4 maxColorValue; // x = 0 (new max to compute, y = old max, the rest uninitialized
	ObjectData objects[];
} objectBuffer;

void main() {
    vec3 lightVal = 
        calculateLighting(sd.lights, sd.ambientColor, fragPos, normal, sd.cameraPos.xyz);

    vec4 tex = texture(tex1, texCoord); 
    vec3 result = lightVal * tex.rgb;


    //float maxVal = objectBuffer.maxColorValue.x;
    //objectBuffer.maxColorValue.x = 
    //uint oldMax = floatBitsToUint(maxVal);
    

    //debugPrintfEXT("My float is %f", uintBitsToFloat(objectBuffer.maxColorValue.x));

    //uint val = floatBitsToUint(dot(vec3(1), result));

    if (pc.data.z == 1) {
        float eps = 0.0001;
         // If difference is not too small, update maximum value
        float res = dot(vec3(1), result);
        if (abs(res - uintBitsToFloat(objectBuffer.maxColorValue.x)) > eps) {
            atomicMax(objectBuffer.maxColorValue.x, floatBitsToUint(res));
        }
    
        float oldMax = uintBitsToFloat(objectBuffer.maxColorValue.y);
        if (oldMax > eps) {
            result = result / oldMax;
        }
    }
   

    if (pc.data.x == 1) { // If enable tone mapping
        switch (pc.data.y) {
        case 0: result = Reinhard(result)  ; break;
        case 1: result = ACESFilm(result)  ; break;
        case 2: result = ACESFitted(result);
        }
    }

    

    //if (gl_FragCoord.x < 0.1 && gl_FragCoord.x > -0.1 && gl_FragCoord.y < 0.1 && gl_FragCoord.y > -0.1) {
    //    result = vec3(objectBuffer.maxColorValue.x);
    //}


    FragColor = vec4(result, tex.a);
}