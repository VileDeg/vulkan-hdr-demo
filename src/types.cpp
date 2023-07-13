#include "stdafx.h"
#include "engine.h"

void Engine::createSamplers() {
	// Create samplers for textures
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VKASSERT(vkCreateSampler(_device, &samplerInfo, nullptr, &_blockySampler));
	_deletionStack.push([=]() {
		vkDestroySampler(_device, _blockySampler, nullptr);
		});

	VkSamplerCreateInfo samplerInfo1 = vkinit::sampler_create_info(VK_FILTER_LINEAR);

	VKASSERT(vkCreateSampler(_device, &samplerInfo1, nullptr, &_linearSampler));
	_deletionStack.push([=]() {
		vkDestroySampler(_device, _linearSampler, nullptr);
		});
}

void Engine::createScene(const std::string mainModelFullPath)
{
	{ // Reset for when we load a scene at runtime
		vkDeviceWaitIdle(_device);

		_renderables.clear();
		_renderContext.lightObjects.clear();
		_models.clear();

		_sceneDisposeStack.flush();

		_meshes.clear();
		_textures.clear();
	}

	// Main model of the scene
	ASSERT(loadModelFromObj("main", mainModelFullPath));

	// Sphere model of the light source
	ASSERT(loadModelFromObj("sphere", Engine::modelPath + "sphere/sphere.obj"));

	// Set materials
	for (auto& [key, model] : _models) {
		for (auto& mesh : model.meshes) {
			mesh->material = getMaterial("general");
		}
	}

	loadCubemap("furry_clouds", true);

	_renderContext.Init();

	if (getModel("sphere")) {
		Model* sphr = getModel("sphere");
		// Light source model should not be affected by light
		sphr->lightAffected = false;
		sphr->useObjectColor = true;

		sphr->meshes[0]->gpuMat.ambientColor  *= 10.f;
		sphr->meshes[0]->gpuMat.diffuseColor  *= 10.f;
		sphr->meshes[0]->gpuMat.specularColor *= 10.f;

		for (int i = 0; i < MAX_LIGHTS; i++) {
			_renderables.push_back(std::make_shared<RenderObject>(
				RenderObject{
					.tag = "light" + std::to_string(i),
					.color = glm::vec4(10, 10, 10, 1.),
					.model = sphr,
					.pos = _renderContext.sceneData.lights[i].position,
					.scale = glm::vec3(0.1f)
				}
			));

			_renderContext.lightObjects.push_back(_renderables.back());
		}
	}

	if (getModel("main")) {
		_renderables.push_back(std::make_shared<RenderObject>(
			RenderObject{
				.model = getModel("main"),
				.pos = glm::vec3(0, -5.f, 0),
				.rot = glm::vec3(0, 90, 0),
				.scale = glm::vec3(15.f)
			}
		));
	}

	_deletionStack.push([this]() {
		_sceneDisposeStack.flush();
	});
}

Material* Engine::createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	auto& mat = _materials[name] = {
		.tag = name,
		.pipeline = pipeline,
		.pipelineLayout = layout
	};

	_deletionStack.push([&]() { mat.cleanup(_device); });

	return &_materials[name];
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

int RenderContext::GetClosestRadiusIndex(int radius) {
	int min_diff = std::numeric_limits<int>::max();

	int closest_index = 0;
	int i = 0;
	for (auto& [key, value] : atten_map) {
		int diff = std::abs(key - radius);
		if (diff < min_diff) {
			min_diff = diff;
			closest_index = i;
		}
		++i;
	}

	return closest_index;
}

void RenderContext::UpdateLightAttenuation(int lightIndex, int mode)
{
	GPULight& l = sceneData.lights[lightIndex];

	// Set the radius to the closest radius that is present in table
	// Find the attenuation values that correspond to the radius
	int ind = GetClosestRadiusIndex(l.radius);
	if (mode == 1) { // Increase
		if (ind != atten_map.size() - 1) {
			++ind;
		}
	} else if (mode == 2) { // Decrease
		if (ind > 0) {
			--ind;
		}
	}
	l.radius = atten_map[ind].first;
	pr("Light radius[" << lightIndex << "] set to: " << l.radius << " units");

	glm::vec3 att = atten_map[ind].second;

	l.constant = att.x;
	l.linear = att.y;
	l.quadratic = att.z;
}

void RenderContext::Init()
{
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

	std::vector<float> radius = { 20.f, 10.f, 30.f, 5.f };
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

	for (int i = 0; i < MAX_LIGHTS; i++) {
		sceneData.lights[i] = {
			.position = lightPos[i],
			.radius = radius[i],

			.color = lightColor[i],

			/*.ambientFactor = 0.1f,
			.diffuseFactor = 1.0f,
			.specularFactor = 0.5f,*/
			.intensity = intensity[i],

			//Attenuation skipped. Will be updated based on radius

			.enabled = enable[i]
		};

		UpdateLightAttenuation(i, 0);
	}

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

Texture* Engine::getTexture(const std::string& name)
{
	return _getCache(name, _textures);
}

Model* Engine::getModel(const std::string& name)
{
	return _getCache(name, _models);
}