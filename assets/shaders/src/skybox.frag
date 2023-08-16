#version 460

layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 uvw; //For skybox cubemap sampling

layout(location = 0) out vec4 FragColor;

#include "incl/defs.glsl"
#include "incl/light.glsl"
#include "incl/bump_mapping.glsl" 

layout(set = 0, binding = 1)
#include "incl/sceneUB.incl" 
ub;

layout(set = 0, binding = 3) uniform samplerCube skybox;

void main()  
{
    if (ub.showNormals) {
        FragColor = vec4(normal, 1.f);
        return;
    }

    vec3 result = vec3(0);
    result = texture(skybox, uvw).rgb;
   
    FragColor = vec4(result, 1.f);
}