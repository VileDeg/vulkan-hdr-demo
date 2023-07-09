#version 460

layout(location = 0) in vec3 vPosition;

// Unused
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec2 vTexCoord;

layout (location = 0) out vec4 fragPos;
layout (location = 1) out flat vec3 lightPos;

#include "defs.glsl"

struct ObjectData{
	mat4 model;
	vec4 color;

	int useObjectColor;
    int _pad0;
    int _pad1;
    int _pad2;
};

layout (set = 0, binding = 0) uniform UBO 
{
	mat4 projection;
	vec4 lightPos;
} ubo;

layout(std430, set = 0, binding = 1) buffer GlobalBuffer{
	ObjectData objects[MAX_OBJECTS];
} ssbo;

layout(push_constant) uniform PushConsts 
{
	mat4 view;
	float far_plane;
} pushConsts;
 
out gl_PerVertex 
{
	vec4 gl_Position;
};
 
void main()
{
	mat4 modelMat = ssbo.objects[gl_BaseInstance].model;
	
	fragPos = modelMat * vec4(vPosition, 1.0);	
	lightPos = ubo.lightPos.xyz; 
	
	gl_Position = ubo.projection * pushConsts.view * 
		modelMat * vec4(vPosition, 1.0);
}