#version 460

layout (location = 0) out float outFragColor;

layout (location = 0) in vec4 fragPos;
layout (location = 1) in flat vec3 lightPos;
layout (location = 2) in flat float farPlane;

void main() 
{
	// Store distance to light as 32 bit float value
    vec3 lightVec = fragPos.xyz - lightPos;
    outFragColor = length(lightVec) / farPlane;
    //outFragColor = length(lightVec);
}