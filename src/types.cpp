#include "stdafx.h"
#include "defs.h"
#include "engine.h"

#include "types.h"

void Engine::setDisplayLightSourceObjects(bool display)
{
	if (!display) {
		for (uint32_t i = 0; i < _renderables.size(); ++i) {
			for (auto& l : _renderContext.lightObjects) {
				if (_renderables[i]->tag == l->tag) {
					_renderables.erase(_renderables.begin() + i);
				}
			}
		}
	} else {
		for (auto& l : _renderContext.lightObjects) {
			// If not found
			auto iter = std::find_if(_renderables.begin(), _renderables.end(), [&l](std::shared_ptr<RenderObject> ptr) {
				return ptr->tag == l->tag;
				});
			if (iter == _renderables.end()) {
				_renderables.push_back(l);
			}
		}
	}
}

void GPUData::Reset(FrameData& fd)
{
	camera	 = reinterpret_cast<GPUCameraUB*>(fd.cameraBuffer.memory_ptr);
	scene	 = reinterpret_cast<GPUSceneUB*>(fd.sceneBuffer.memory_ptr);

	ssbo	 = reinterpret_cast<GPUSceneSSBO*>(fd.objectBuffer.memory_ptr);
			 
	compSSBO = reinterpret_cast<GPUCompSSBO*>(fd.compSSBO.memory_ptr);
	compUB	 = reinterpret_cast<GPUCompUB*>(fd.compUB.memory_ptr);
}


RenderContext::RenderContext()
{
	enableSkybox = true;
	displayLightSourceObjects = false;

	fovY = 90.f; // degrees
	zNear = 0.1f;
	zFar = 64.0f;

	showNormals = false;

	// Treshold to calculate light's effective radius for optimization
	lightRadiusTreshold = 1.f / 255.f;

	float off = 5.f;
	std::vector<glm::vec3> lightPos = {
		{ off, 0., off },
		{ off, 0., -off },
		{ -off, 0., off },
		{ -off, 0., -off },
	};

	std::vector<glm::vec3> lightColor = {
		{ 1.f , 1.f , 1.f },
		{ 1.f , 1.f , 1.f },
		{ 1.f , 1.f , 1.f },
		{ 1.f , 1.f , 1.f }
	};

	float amb = 0.05f;
	sceneData.ambientColor = glm::vec4(amb, amb, amb, 1.f);

	sceneData.enableShadows = true;

	sceneData.showShadowMap = false;
	sceneData.shadowBias = 0.15f;
	sceneData.shadowMapDisplayBrightness = 3.5f;

	sceneData.enablePCF = true;

	for (int i = 0; i < MAX_LIGHTS; i++) {
		auto& l = sceneData.lights[i];

		l.color = lightColor[i];
		l.constant = 1.f;
		l.linear = 0.22f;
		l.quadratic = 0.2f;

		UpdateLightRadius(i);
	}

	sceneData.lightProjMat = glm::perspective(glm::radians(90.f), 1.0f, zNear, zFar);
}

void RenderContext::UpdateLightPosition(int lightIndex, glm::vec3 newPos)
{
	sceneData.lights[lightIndex].position = newPos;
	lightObjects[lightIndex]->pos = newPos;
}

void RenderContext::UpdateLightRadius(int i)
{
	float Kc = sceneData.lights[i].constant;
	float Kl = sceneData.lights[i].linear;
	float Kq = sceneData.lights[i].quadratic;

	float LC = sceneData.lights[i].intensity;
	float termC = -LC / lightRadiusTreshold + Kc;

	float rootFrom = Kl * Kl - 4 * Kq * termC;

	sceneData.lights[i].radius = -Kl + std::sqrt(rootFrom);
}


glm::mat4 RenderObject::Transform() 
{
	ASSERT(model != nullptr);
	glm::vec3 scales = scale;
	if (model != nullptr) {
		scales *= glm::vec3(1.f / model->maxExtent);
	}

	return
		glm::translate(glm::mat4(1.f), pos)	   *
		glm::rotate(glm::radians(rot.x), glm::vec3(1, 0, 0)) *
		glm::rotate(glm::radians(rot.y), glm::vec3(0, 1, 0)) *
		glm::rotate(glm::radians(rot.z), glm::vec3(0, 0, 1)) *
		glm::scale(glm::mat4(1.f), scales);
}

bool RenderObject::HasMoved() 
{
	bool hasMoved = pos != _prevPos;
	_prevPos = pos;

	return hasMoved;
}


VertexInputDescription Vertex::getDescription()
{
	VertexInputDescription description;

	VkVertexInputBindingDescription bindingDescription = {
		.binding = 0,
		.stride = sizeof(Vertex),
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

	description.bindings.push_back(bindingDescription);

	VkVertexInputAttributeDescription positionAttribute = {
		.location = 0,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, pos),
	};

	VkVertexInputAttributeDescription normalAttribute = {
		.location = 1,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, normal),
	};

	VkVertexInputAttributeDescription colorAttribute = {
		.location = 2,
		.binding = 0,
		.format = VK_FORMAT_R32G32B32_SFLOAT,
		.offset = offsetof(Vertex, color),
	};

	VkVertexInputAttributeDescription uvAttribute = {
		.location = 3,
		.binding = 0,
		.format = VK_FORMAT_R32G32_SFLOAT,
		.offset = offsetof(Vertex, uv)
	};

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(uvAttribute);

	return description;
}


template<typename T>
static T* get_cache(const std::string& name, std::unordered_map<std::string, T>& map)
{
	//search for the object, and return nullptr if not found
	auto it = map.find(name);
	if (it == map.end()) {
		//ASSERT(false);
		return nullptr;
	} else {
		return &(*it).second;
	}
}

Material* Engine::getMaterial(const std::string& name)
{
	return get_cache(name, _materials);
}

Mesh* Engine::getMesh(const std::string& name)
{
	return get_cache(name, _meshes);
}

Attachment* Engine::getTexture(const std::string& name)
{
	return get_cache(name, _textures);
}

Model* Engine::getModel(const std::string& name)
{
	return get_cache(name, _models);
}