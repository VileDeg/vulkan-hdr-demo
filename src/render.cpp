#include "stdafx.h"
#include "Engine.h"

Material* Engine::createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	auto& mat = _materials[name] = {
		.pipeline = pipeline,
		.pipelineLayout = layout
	};

	_deletionStack.push([&]() { mat.cleanup(_device); });

	return &_materials[name];
}

Material* Engine::getMaterial(const std::string& name)
{
	//search for the object, and return nullptr if not found
	auto it = _materials.find(name);
	if (it == _materials.end()) {
		return nullptr;
	} else {
		return &(*it).second;
	}
}

Mesh* Engine::getMesh(const std::string& name)
{
	auto it = _meshes.find(name);
	if (it == _meshes.end()) {
		return nullptr;
	} else {
		return &(*it).second;
	}
}

void Engine::bindPipeline(VkCommandBuffer commandBuffer, VkPipeline pipeline)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	VkViewport viewport{
		.x = 0.0f,
		.y = 0.0f,
		.width = (float)_windowExtent.width,
		.height = (float)_windowExtent.height,
		.minDepth = 0.0f,
		.maxDepth = 1.0f
	};

	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = _windowExtent
	};

	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Engine::drawObjects(VkCommandBuffer cmd, const std::vector<RenderObject>& objects)
{
	glm::mat4 projMat = glm::perspective(glm::radians(45.f), _windowExtent.width / (float)_windowExtent.height, 0.01f, 200.f);
	projMat[1][1] *= -1; //Flip y-axis

	float framed = _frameNumber / 120.f;
	_sceneParameters.ambientColor = { sin(framed)*0.5f, 0.5f, cos(framed)*0.5f, 1.0f};

	char* sceneData;
	vmaMapMemory(_allocator, _sceneParameterBuffer.allocation, (void**)&sceneData);
	sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;
	memcpy(sceneData, &_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(_allocator, _sceneParameterBuffer.allocation);

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < objects.size(); i++) {
        const RenderObject& obj = objects[i];

		GPUCameraData camData{
			.view = _inp.camera.GetViewMat(),
			.proj = projMat,
			.viewproj = projMat * _inp.camera.GetViewMat()
		};
		void* data;
		vmaMapMemory(_allocator, getCurrentFrame().cameraBuffer.allocation, &data);
		memcpy(data, &camData, sizeof(GPUCameraData));
		vmaUnmapMemory(_allocator, getCurrentFrame().cameraBuffer.allocation);

		if (obj.material != lastMaterial) {
			bindPipeline(cmd, obj.material->pipeline);
			lastMaterial = obj.material;

			//offset for our scene buffer
			uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, obj.material->pipelineLayout, 0, 1, &getCurrentFrame().globalDescriptor, 1, &uniform_offset);

			if (obj.material->textureSet != VK_NULL_HANDLE) {
				//texture descriptor
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, obj.material->pipelineLayout, 1, 1, &obj.material->textureSet, 0, nullptr);

			}
		}

		if (obj.mesh != lastMesh) {
			VkDeviceSize zeroOffset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &obj.mesh->_vertexBuffer.buffer, &zeroOffset);
			lastMesh = obj.mesh;
		}
		
        glm::mat4 modelMat = obj.transform;

		MeshPushConstants pushConsts{
			.render_matrix = modelMat
		};
		ASSERT(obj.material && obj.mesh);

        vkCmdPushConstants(cmd, obj.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &pushConsts);
        vkCmdDraw(cmd, obj.mesh->_vertices.size(), 1, 0, 0);
    }
}



void Engine::loadTextures()
{
	Texture lostEmpire;

	loadImageFromFile(Engine::imagePath + "lost_empire-RGBA.png", lostEmpire.image);

	VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, lostEmpire.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
	VKASSERT(vkCreateImageView(_device, &imageinfo, nullptr, &lostEmpire.imageView));
	_deletionStack.push([=]() {
		vkDestroyImageView(_device, lostEmpire.imageView, nullptr);
		});

	_loadedTextures["empire_diffuse"] = lostEmpire;
}

void Engine::loadMeshes()
{
	uint32_t vertexCount = 12;

	_meshes["triangle"] = {};
	Mesh& triMesh = _meshes["triangle"];

	triMesh._vertices.resize(vertexCount);

	//back
	triMesh._vertices[0].pos = { 1.f, 1.f, 0.0f };
	triMesh._vertices[1].pos = { -1.f, 1.f, 0.0f };
	triMesh._vertices[2].pos = { 0.f, -1.f, 0.0f };

	float z = -2.f;

	//boottom
	triMesh._vertices[3].pos = { 0.f, 1.f, z };
	triMesh._vertices[4].pos = { -1.f, 1.f, 0.0f };
	triMesh._vertices[5].pos = { 1.f, 1.f, 0.0f };

	//right
	triMesh._vertices[6].pos = { 0.f, 1.f, z };
	triMesh._vertices[7].pos = { 1.f, 1.f, 0.0f };
	triMesh._vertices[8].pos = { 0.f, -1.f, 0.0f };

	//left
	triMesh._vertices[9].pos = { 0.f, 1.f, z };
	triMesh._vertices[10].pos = { 0.f, -1.f, 0.0f };
	triMesh._vertices[11].pos = { -1.f, 1.f, 0.0f };

	std::vector<glm::vec3> colors = {
		{ 1.f, 0.f, 0.f },
		{ 0.f, 1.f, 0.f },
		{ 0.f, 0.f, 1.f },
	};

	for (uint32_t i = 0; i < vertexCount; ++i) {
		triMesh._vertices[i].color = colors[i % 3];
	}

	_meshes["model"] = {};
	Mesh& modelMesh = _meshes["model"];

	modelMesh.loadFromObj(Engine::modelPath + "lost_empire.obj");

	uploadMesh(triMesh);
	uploadMesh(modelMesh);
}


void Engine::createScene()
{
	

	//Rotate to face the camera
	glm::mat4 modelMat = glm::mat4(1.f);
	modelMat = glm::translate(modelMat, glm::vec3(0, 2.5f, 0));
	modelMat = glm::rotate(modelMat, glm::radians(180.f), glm::vec3(0, 1, 0));
	RenderObject model{
		.mesh = getMesh("model"),
		.material = getMaterial("default"),
		.transform = modelMat
	};

	_renderables.push_back(model);
	/*for (int x = -20; x < 20; x++) {
		for (int y = -20; y < 20; y++) {
			glm::mat4 modelMat = glm::mat4(1.f);
			modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, y));
			float sf = 0.2f;
			modelMat = glm::scale(modelMat, glm::vec3(sf, sf, sf));
			RenderObject tri{
				.mesh = getMesh("triangle"),
				.material = getMaterial("default"),
				.transform = modelMat
			};
			_renderables.push_back(tri);
		}
	}*/


	//create a sampler for the texture
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSampler blockySampler;
	VKASSERT(vkCreateSampler(_device, &samplerInfo, nullptr, &blockySampler));
	_deletionStack.push([=]() {
		vkDestroySampler(_device, blockySampler, nullptr);
		});

	Material* material = getMaterial("default");

	//allocate the descriptor set for single-texture to use on the material
	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = _descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &_singleTextureSetLayout
	};

	VKASSERT(vkAllocateDescriptorSets(_device, &allocInfo, &material->textureSet));


	//write to the descriptor set so that it points to our empire_diffuse texture
	VkDescriptorImageInfo imageBufferInfo{
		.sampler = blockySampler,
		.imageView = _loadedTextures["empire_diffuse"].imageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkWriteDescriptorSet texture1 = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, material->textureSet, &imageBufferInfo, 0);

	vkUpdateDescriptorSets(_device, 1, &texture1, 0, nullptr);
}
