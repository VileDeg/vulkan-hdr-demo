#include "stdafx.h"
#include "engine.h"

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
			if (std::find_if(_renderables.begin(), _renderables.end(), [&l](std::shared_ptr<RenderObject> ptr)
				{
					return ptr->tag == l->tag;
				}) == _renderables.end()) 
			{
				_renderables.push_back(l);
			}
		}
	}
}


void GPUData::Reset(FrameData& fd)
{
	camera	 = reinterpret_cast<GPUCameraUB*>(fd.cameraBuffer.gpu_ptr);
	scene	 = reinterpret_cast<GPUSceneUB*>(fd.sceneBuffer.gpu_ptr);

	ssbo	 = reinterpret_cast<GPUSceneSSBO*>(fd.objectBuffer.gpu_ptr);
			 
	compSSBO = reinterpret_cast<GPUCompSSBO*>(fd.compSSBO.gpu_ptr);
	compUB	 = reinterpret_cast<GPUCompUB*>(fd.compUB.gpu_ptr);
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

void RenderContext::Init(CreateSceneData data)
{
	modelName  = data.modelPath;
	skyboxName = data.skyboxPath;

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

	//std::vector<float> radius = { 20.f, 10.f, 30.f, 5.f };
	std::vector<float> intensity = { 10.f, 5.f, 3.f, 1.f };
	//std::vector<float> intensity = { 1.f, 1.f, 1.f, 1.f };
	std::vector<bool> enable = { true, true, true, true };


	float amb = 0.1f;
	sceneData.ambientColor = glm::vec4(amb, amb, amb, 1.f);

	sceneData.enableShadows = true;

	sceneData.showShadowMap = false;
	sceneData.shadowBias = 0.15f;
	sceneData.shadowMapDisplayBrightness = 3.5f;

	sceneData.enablePCF = true;

	sceneData.bumpStrength = data.bumpStrength;

	for (int i = 0; i < MAX_LIGHTS; i++) {
		sceneData.lights[i] = {
			.position = data.position[i],

			.color = lightColor[i],
			.intensity = data.intensity[i],

			.constant = 1.f,
			.linear = 0.22f,
			.quadratic = 0.2f,

			.enabled = enable[i]
		};

		UpdateLightRadius(i);
	}

	//std::vector<bool> enable = { true, true, true, true };
	//bool a = true;
	//GPUBool b = a;
	//b = enable[0]; // <- This produces warning

	sceneData.lightProjMat = glm::perspective(glm::radians(90.f), 1.0f, zNear, zFar);
}

glm::mat4 RenderObject::Transform() {
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

bool RenderObject::HasMoved() {
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


namespace {
	template<typename T>
	T* _getCache(const std::string& name, std::unordered_map<std::string, T>& map)
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
}

Material* Engine::getMaterial(const std::string& name)
{
	return _getCache(name, _materials);
}

Mesh* Engine::getMesh(const std::string& name)
{
	return _getCache(name, _meshes);
}

Attachment* Engine::getTexture(const std::string& name)
{
	return _getCache(name, _textures);
}

Model* Engine::getModel(const std::string& name)
{
	return _getCache(name, _models);
}