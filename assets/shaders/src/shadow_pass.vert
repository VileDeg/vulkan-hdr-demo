#version 460

layout(location = 0) in vec3 vPosition;

// Unused
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec2 vTexCoord;

layout (location = 0) out vec4 fragPos;
layout (location = 1) out flat vec3 lightPos;

#include "incl/defs.glsl"
#include "incl/scene_structs.incl"

layout(set = 0, binding = 0)
#include "incl/sceneUB.incl"
sd;

layout(std430, set = 0, binding = 1)
#include "incl/objectSSBO.incl" 
ssbo;

layout(push_constant) uniform PushConsts 
{
	mat4 view;
	float far_plane;
    uint lightIndex;
} pc;
 
void main()
{
	mat4 modelMat = ssbo.objects[gl_BaseInstance].model;
	
	fragPos = modelMat * vec4(vPosition, 1.0);	
	//lightPos = ubo.lightPos.xyz; 
    lightPos = sd.lights[pc.lightIndex].pos;
	
	gl_Position = sd.lightProjMat * pc.view * 
		modelMat * vec4(vPosition, 1.0);
}