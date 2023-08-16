#version 460

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec2 vTexCoord;

layout(location = 0) out vec3 normal;
layout(location = 1) out vec3 uvw; //For cubemap sampling

#include "incl/defs.glsl"

layout(set = 0, binding = 0) uniform CameraBuffer {
	mat4 view;
	mat4 proj;
	mat4 viewproj;
} cam;

layout(std430, set = 0, binding = 2)
#include "incl/objectSSBO.incl" 
ssbo;

#include "incl/scenePC.incl" 
pc;

void main()
{
	gl_Position = (cam.proj * mat4(mat3(cam.view)) * vec4(vPosition, 1.0f)).xyww; 

	normal = normalize(mat3(ssbo.objects[pc.objectIndex].normalMatrix) * vNormal);

	uvw = vPosition;
}

