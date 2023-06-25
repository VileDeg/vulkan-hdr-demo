#version 460

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec2 vTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 texCoord;
layout(location = 3) out flat vec4 objectColor;
layout(location = 4) out vec3 normal;

layout(location = 5) out vec3 uvw; //For cubemap sampling

#include "defs.glsl"

#include "structs.glsl"

layout(set = 0, binding = 0) uniform CameraBuffer {
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 position;
} cam;

layout(push_constant) uniform PushConstants {
    int hasTextures;
    int lightAffected;
    int isCubemap;
    int _pad0;
} pc;

void main()
{
	mat4 modelMat = ssbo.objects[gl_BaseInstance].model;
	mat4 transformMat = cam.viewproj * modelMat;
	
	if (pc.isCubemap == 0) {
		gl_Position = transformMat * vec4(vPosition, 1.0f);
	} else {
		gl_Position = (cam.proj * mat4(mat3(cam.view)) * vec4(vPosition, 1.0f)).xyww; 
	}

	fragPos = vec3(modelMat * vec4(vPosition, 1.0f));
	fragColor = vColor;
	texCoord = vTexCoord;
	objectColor = ssbo.objects[gl_BaseInstance].color;
	normal = mat3(transpose(inverse(modelMat))) * vNormal;

	uvw = vPosition;
	//uvw.xy *= -1.0;
}
