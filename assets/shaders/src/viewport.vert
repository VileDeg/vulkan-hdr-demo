#version 460

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec2 vTexCoord;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 texCoord;
layout(location = 3) out vec3 normal;
layout(location = 4) out vec3 uvw; //For cubemap sampling

#include "incl/defs.glsl"

layout(set = 0, binding = 0) uniform CameraBuffer {
	mat4 view;
	mat4 proj;
	mat4 viewproj;
} cam;

layout(std430, set = 0, binding = 2)
#include "incl/objectSSBO.incl" 
ssbo;

layout(push_constant) uniform ScenePC {
    bool lightAffected;
    bool isCubemap;
	uint objectIndex;
    uint meshIndex;
	bool useDiffTex;
	bool useBumpTex;
} pc;

void main()
{
	mat4 modelMat = ssbo.objects[pc.objectIndex].model;
	mat4 transformMat = cam.viewproj * modelMat;
	
	if (!pc.isCubemap) {
		gl_Position = transformMat * vec4(vPosition, 1.0f);
	} else {
		gl_Position = (cam.proj * mat4(mat3(cam.view)) * vec4(vPosition, 1.0f)).xyww; 
	}

	fragPos = vec3(modelMat * vec4(vPosition, 1.0f));
	fragColor = vColor;
	texCoord = vTexCoord;
	//objectColor = ssbo.objects[pc.objectIndex].color;

	//normalMat = mat3(transpose(inverse(modelMat)));
	normal = normalize(mat3(ssbo.objects[pc.objectIndex].normalMatrix) * vNormal);

	uvw = vPosition;
}

