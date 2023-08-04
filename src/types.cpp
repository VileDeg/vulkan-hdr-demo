#include "stdafx.h"
#include "engine.h"

#include "vk_descriptors.h"

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

			.enabled = (bool)enable[i]
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


void ComputeStage::Create(VkDevice device, VkSampler sampler,
	const std::string& shaderBinName, bool usePushConstants/* = false*/)
{
	this->device = device;
	this->sampler = sampler;

	ShaderData comp;
	comp.code = utils::readShaderBinary(Engine::SHADER_PATH + shaderBinName);

	if (utils::createShaderModule(device, comp.code, &comp.module)) {
		std::cout << "Compute shader successfully loaded." << std::endl;
	} else {
		PRWRN("Failed to load compute shader");
	}

	VkPipelineShaderStageCreateInfo stageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = comp.module,
		.pName = "main"
	};

	VkPipelineLayoutCreateInfo layoutInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &setLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};

	if (usePushConstants) {
		VkPushConstantRange pcRange = {
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.offset = 0,
			.size = sizeof(GPUCompPC),
		};

		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pcRange;
	}

	VKASSERT(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

	VkComputePipelineCreateInfo computePipelineInfo{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = stageInfo,
		.layout = pipelineLayout,
	};

	VKASSERT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &pipeline));

	vkDestroyShaderModule(device, comp.module, nullptr);

	imageBindings.reserve(MAX_IMAGE_UPDATES);
}

ComputeStage& ComputeStage::Bind(VkCommandBuffer cmd) 
{
	commandBuffer = cmd;
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	return *this;
}

ComputeStage& ComputeStage::UpdateImage(VkImageView view, uint32_t binding) 
{
	imageBindings.push_back({ { view }, binding });
	ASSERT(imageBindings.size() <= MAX_IMAGE_UPDATES);

	return *this;
}

ComputeStage& ComputeStage::UpdateImage(Attachment att, uint32_t binding) 
{
	imageBindings.push_back({ { att.view }, binding });
	ASSERT(imageBindings.size() <= MAX_IMAGE_UPDATES);

	return *this;
}

ComputeStage& ComputeStage::UpdateImagePyramid(AttachmentPyramid& att, uint32_t binding)
{
	imageBindings.push_back({ att.views, binding });
	ASSERT(imageBindings.size() <= MAX_IMAGE_UPDATES);

	return *this;
}

ComputeStage& ComputeStage::WriteSets(int set_i)
{
	if (!imageBindings.empty()) {
		// Need to create vector for image infos to hold data until update command is executed
		std::vector<std::vector<VkDescriptorImageInfo>> imageInfos;
		imageInfos.resize(imageBindings.size());

		std::vector<VkWriteDescriptorSet> writes;
		int i = 0;
		for (auto& ib : imageBindings) {
			for (auto& v : ib.views) {
				VkDescriptorImageInfo imgInfo{
					.sampler = sampler,
					.imageView = v,
					.imageLayout = VK_IMAGE_LAYOUT_GENERAL
				};

				imageInfos[i].push_back(imgInfo);
			}

			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.pNext = nullptr;

			write.dstBinding = ib.binding;
			write.dstSet = sets[set_i];
			write.descriptorCount = imageInfos[i].size();
			write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			write.pImageInfo = imageInfos[i].data();

			writes.push_back(write);
			++i;
		}

		vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
		imageBindings.clear();
	}

	return *this;
}

ComputeStage& ComputeStage::Dispatch(uint32_t groupsX, uint32_t groupsY, int set_i) 
{
	WriteSets(set_i);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &sets[set_i], 0, nullptr);
	vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);
	
	return *this;
}

void ComputeStage::Barrier() 
{
	utils::memoryBarrier(commandBuffer,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void ComputeStage::InitDescriptorSets(
	DescriptorLayoutCache* dLayoutCache, DescriptorAllocator* dAllocator,
	FrameData& f, int frame_i)
{
	ASSERT(!dsetBindings.empty());

	DescriptorBuilder builder = DescriptorBuilder::begin(dLayoutCache, dAllocator);

	for (int i = 0; i < dsetBindings.size(); ++i) {
		switch (dsetBindings[i]) {
		case UB:
			builder.bind_buffer(i, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
			break;
		case SSBO:
			builder.bind_buffer(i, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
			break;
		case IMG:
			builder.bind_image_empty(i, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
			break;
		case PYR:
			builder.bind_image_empty(i, MAX_VIEWPORT_MIPS, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
			break;
		default:
			ASSERT(false);
			break;
		}
	}

	builder.build(sets[frame_i], setLayout);
}

void ComputeStage::Destroy() 
{
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
}

ExposureFusion::ExposureFusion() 
{
	stages["0"] = { .shaderName = "ltm_fusion_0.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG, IMG} };
	stages["1"] = { .shaderName = "ltm_fusion_1.comp.spv", .dsetBindings = { UB, PYR, PYR }, .usesPushConstants = true };
	stages["2"] = { .shaderName = "ltm_fusion_2.comp.spv", .dsetBindings = { UB, PYR, PYR, PYR} };
	//stages["3"] = { .shaderName = "ltm_fusion_3.comp.spv", .dsetBindings = { UB, PYR, IMG} };
	stages["4"] = { .shaderName = "ltm_fusion_4.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG} };

	// TODO: change naming to "_filter_horiz/vert"
	stages["upsample0_sub"] = { .shaderName = "upsample.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["upsample1_sub"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["upsample2_sub"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };

	stages["upsample0_add"] = { .shaderName = "upsample.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["upsample1_add"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["upsample2_add"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };

	stages["add"]	   = { .shaderName = "mipmap_add.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["subtract"] = { .shaderName = "mipmap_subtract.comp.spv", .dsetBindings = { UB, PYR, PYR, PYR}, .usesPushConstants = true };

	stages["move"] = { .shaderName = "move.comp.spv", .dsetBindings = { UB, IMG, IMG }};

	// TODO: change naming to "_filter_horiz/vert"
	stages["downsample0_lum"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["downsample1_lum"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["downsample2_lum"] = { .shaderName = "decimate.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };

	stages["downsample0_weight"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["downsample1_weight"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["downsample2_weight"] = { .shaderName = "decimate.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };

	att["chrom"] = att["laplacSum"] = {};

	pyr["lum"] = pyr["weight"] = pyr["laplac"] = pyr["blendedLaplac"] = {};
	// Pyramid that will hold intermediate filtered images when upsampling
	pyr["upsampled0"] = pyr["upsampled1"] = {};
	// Pyramid that will hold intermediate filtered images when downsampling
	pyr["downsampled0"] = pyr["downsampled1"] = {};
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