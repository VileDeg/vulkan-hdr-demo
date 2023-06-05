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
	// Load objects' SSBO to GPU
	getCurrentFrame().objectBuffer.runOnMemoryMap(_allocator, [&](void* data) {
		glm::uvec4* maxVal = (glm::uvec4*)data;
		if (_frameNumber == 0) { // Initially new MAX and old MAX are zero.
			*maxVal = glm::uvec4(0, 0, 0, 0);
		} else { // On every next frame, swap new MAX and old MAX and set new MAX to zero.
			std::swap(maxVal->x, maxVal->y);
			maxVal->x = 0;
		}
		
		++maxVal;
        GPUObjectData* objectSSBO = (GPUObjectData*)maxVal;

		for (int i = 0; i < objects.size(); i++) {
			objectSSBO[i].modelMatrix = objects[i].transform;
			objectSSBO[i].color = objects[i].color;
			
			//objectSSBO[i].color = glm::vec4{ sin(i), cos(i), sin(-i), 1.0f };
			/*if (objects[i].tag == "light") {
				objectSSBO[i].color = glm::vec4{ 1.0f, 1.0f, 1.0f, 1.0f };
			} else {
				objectSSBO[i].color = glm::vec4{ sin(i), cos(i), sin(-i), 1.0f };
			}*/
        }
    });

	float framed = _frameNumber / 120.f;
	//_sceneParameters.ambientColor = { sin(framed) * 0.5f, 0.5f, cos(framed) * 0.5f, 1.0f };
	//_sceneParameters.ambientColor = glm::vec4(1.f, 1.f, 1.f, 1.f);
	_renderContext.sceneData.cameraPos = _inp.camera.GetPos();

	// Load scene data to GPU
	
	_sceneParameterBuffer.runOnMemoryMap(_allocator, [&](void* data) {
		//auto layout = _renderContext.sceneData.GPULayout();
		//size_t bytes_size = layout.size() * sizeof(glm::vec4);
		char* sceneData = (char*)data;
		sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;
		memcpy(sceneData, &_renderContext.sceneData, sizeof(GPUSceneData));
	});

	glm::mat4 projMat = glm::perspective(glm::radians(_fovY), _windowExtent.width / (float)_windowExtent.height, 0.01f, 200.f);
	projMat[1][1] *= -1; //Flip y-axis

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < objects.size(); i++) {
        const RenderObject& obj = objects[i];

		GPUCameraData camData{
			.view = _inp.camera.GetViewMat(),
			.proj = projMat,
			.viewproj = projMat * _inp.camera.GetViewMat(),
			
		};
		getCurrentFrame().cameraBuffer.runOnMemoryMap(_allocator, [&](void* data) {
			memcpy(data, &camData, sizeof(GPUCameraData));
		});

		// If the material is different, bind the new material
		if (obj.material != lastMaterial) {
			bindPipeline(cmd, obj.material->pipeline);
			lastMaterial = obj.material;

			//offset for our scene buffer
			uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;

			// Always add the global and object descriptor
			std::vector<VkDescriptorSet> sets = { getCurrentFrame().globalDescriptor, getCurrentFrame().objectDescriptor};
			// If the material has a texture, add texture descriptor
			if (obj.material->textureSet != VK_NULL_HANDLE) {
				sets.push_back(obj.material->textureSet);
            }
			// Bind the descriptor sets
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, obj.material->pipelineLayout, 0, sets.size(), sets.data(), 1, &uniform_offset);
		}

		if (obj.mesh != lastMesh) {
			VkDeviceSize zeroOffset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &obj.mesh->_vertexBuffer.buffer, &zeroOffset);
			lastMesh = obj.mesh;
		}
		
        glm::mat4 modelMat = obj.transform;

		MeshPushConstants pushConsts{
			.data = { _inp.toneMappingEnabled, _toneMappingOp, _inp.exposureEnabled, _toneMappingOp },
			.render_matrix = modelMat
		};
		ASSERT(obj.material && obj.mesh);

        vkCmdPushConstants(cmd, obj.material->pipelineLayout, 
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
			0, sizeof(MeshPushConstants), &pushConsts);

		// Ve send loop index as instance index to use it in shader to access object data in SSBO
        vkCmdDraw(cmd, obj.mesh->_vertices.size(), 1, 0, i);
    }
}

void Engine::loadTextures()
{
	Texture lostEmpire;

	ASSERT(loadImageFromFile(Engine::imagePath + "lost_empire/lost_empire-RGBA.png", lostEmpire.image));

	VkImageViewCreateInfo imageinfo = vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, lostEmpire.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
	VKASSERT(vkCreateImageView(_device, &imageinfo, nullptr, &lostEmpire.imageView));
	_deletionStack.push([=]() {
		vkDestroyImageView(_device, lostEmpire.imageView, nullptr);
		});

	_loadedTextures["empire_diffuse"] = lostEmpire;
}

void Engine::loadMeshes()
{
	_meshes["ball"] = {};
	Mesh& ballMesh = _meshes["ball"];
	ASSERT(ballMesh.loadFromObj(Engine::modelPath + "CustomBall/CustomBall.obj"));

	_meshes["model"] = {};
	Mesh& modelMesh = _meshes["model"];

	ASSERT(modelMesh.loadFromObj(Engine::modelPath + "lost_empire/lost_empire.obj"));
	//modelMesh.loadFromGLTF(Engine::modelPath + "back_rooms/scene.gltf");

	uploadMesh(ballMesh);
	uploadMesh(modelMesh);
}


void Engine::createScene()
{
	glm::mat4 modelMat = glm::mat4(1.f);

	_renderContext.Init();
	for (int i = 0; i < MAX_LIGHTS; i++) {
		auto lightMat = modelMat;
		lightMat = glm::translate(lightMat, _renderContext.sceneData.lights[i].position);
		lightMat = glm::scale(lightMat, glm::vec3(10.f));

		RenderObject lightSource{
			.tag = "light" + std::to_string(i),
			//.color = _renderContext.lightColor[i] * _renderContext.intensity[i],
			.color = glm::vec4(0.5, 0.5, 0.5, 1.),
			.mesh = getMesh("ball"),
			.material = getMaterial("colored"),
			.transform = lightMat
		};
		_renderables.push_back(lightSource);
    }

	Material* mat = getMaterial("textured");

	//Rotate to face the camera
	
	modelMat = glm::translate(modelMat, glm::vec3(0, -18.f, 0));
	//modelMat = glm::rotate(modelMat, glm::radians(180.f), glm::vec3(0, 1, 0));
	RenderObject model{
		.mesh = getMesh("model"),
		.material = mat,
		.transform = modelMat
	};

	_renderables.push_back(model);

#if 0
	for (int x = -20; x < 20; x++) {
		for (int y = -20; y < 20; y++) {
			glm::mat4 modelMat = glm::mat4(1.f);
			modelMat = glm::translate(glm::mat4(1.0f), glm::vec3(x, 20.0f, y));
			float sf = 0.2f;
			modelMat = glm::scale(modelMat, glm::vec3(sf, sf, sf));
			RenderObject tri{
				.mesh = getMesh("triangle"),
				.material = getMaterial("colored"),
				.transform = modelMat
			};
			_renderables.push_back(tri);
		}
	}
#endif

	/*_renderContext.lightSource = lightSource;
	_renderContext.lightPos = glm::vec4(0, 40.f, 0, 1.f);*/

	//create a sampler for the texture
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VkSampler blockySampler;
	VKASSERT(vkCreateSampler(_device, &samplerInfo, nullptr, &blockySampler));
	_deletionStack.push([=]() {
		vkDestroySampler(_device, blockySampler, nullptr);
		});

	

	//allocate the descriptor set for single-texture to use on the material
	VkDescriptorSetAllocateInfo allocInfo{
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = _descriptorPool,
		.descriptorSetCount = 1,
		.pSetLayouts = &_singleTextureSetLayout
	};

	VKASSERT(vkAllocateDescriptorSets(_device, &allocInfo, &mat->textureSet));


	//write to the descriptor set so that it points to our empire_diffuse texture
	VkDescriptorImageInfo imageBufferInfo{
		.sampler = blockySampler,
		.imageView = _loadedTextures["empire_diffuse"].imageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkWriteDescriptorSet texture1 = 
		vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
			mat->textureSet, &imageBufferInfo, 0);

	vkUpdateDescriptorSets(_device, 1, &texture1, 0, nullptr);
}

//GPUSceneData::GPUSceneData(glm::vec4 ambCol,
//	std::vector<glm::vec3> lightPos,
//	std::vector<glm::vec4> lightColor,
//	std::vector<float> radius,
//	std::vector<float> intensity)
//	: cameraPos(0.f)
//{
//	static std::unordered_map<int, glm::vec3> atten_map = {
//		// From: https://learnopengl.com/Lighting/Light-casters
//		{ 7   , {1.0, 0.7   , 1.8     } },
//		{ 13  , {1.0, 0.35  , 0.44    } },
//		{ 20  , {1.0, 0.22  , 0.20    } },
//		{ 32  , {1.0, 0.14  , 0.07    } },
//		{ 50  , {1.0, 0.09  , 0.032   } },
//		{ 65  , {1.0, 0.07  , 0.017   } },
//		{ 100 , {1.0, 0.045 , 0.0075  } },
//		{ 160 , {1.0, 0.027 , 0.0028  } },
//		{ 200 , {1.0, 0.022 , 0.0019  } },
//		{ 325 , {1.0, 0.014 , 0.0007  } },
//		{ 600 , {1.0, 0.007 , 0.0002  } },
//		{ 3250, {1.0, 0.0014, 0.000007} }
//	};
//
//	ambientColor = ambCol;
//	for (int i = 0; i < MAX_LIGHTS; i++) {
//		// Find the attenuation values that are the most fitting for the specified radius
//		int min_diff = std::numeric_limits<int>::max();
//		int closest_key = 0;
//
//		for (auto& [key, value] : atten_map) {
//			int diff = std::abs(key - radius[i]);
//			if (diff < min_diff) {
//				min_diff = diff;
//				closest_key = key;
//			}
//		}
//		radius[i] = closest_key;
//		pr("Light radius[" << i << "] set to: " << radius[i] << " units");
//		glm::vec3 att = atten_map[closest_key];
//
//		light[i] = {
//			.color = { lightColor[i] },
//			.pos = glm::vec4(lightPos[i], radius[i]), 
//			.fac = { 0.1, 1.0, 0.5, intensity[i]}, 
//			.att = glm::vec4(att, 0.f)
//		};
//	}
//}


//RenderContext::RenderContext() {
//	Init();
//};


void RenderContext::UpdateLightAttenuation(int lightIndex)
{
	static std::unordered_map<int, glm::vec3> atten_map = {
		// From: https://learnopengl.com/Lighting/Light-casters
		{ 7   , {1.0, 0.7   , 1.8     } },
		{ 13  , {1.0, 0.35  , 0.44    } },
		{ 20  , {1.0, 0.22  , 0.20    } },
		{ 32  , {1.0, 0.14  , 0.07    } },
		{ 50  , {1.0, 0.09  , 0.032   } },
		{ 65  , {1.0, 0.07  , 0.017   } },
		{ 100 , {1.0, 0.045 , 0.0075  } },
		{ 160 , {1.0, 0.027 , 0.0028  } },
		{ 200 , {1.0, 0.022 , 0.0019  } },
		{ 325 , {1.0, 0.014 , 0.0007  } },
		{ 600 , {1.0, 0.007 , 0.0002  } },
		{ 3250, {1.0, 0.0014, 0.000007} }
	};

	Light& l = sceneData.lights[lightIndex];

	// Set the radius to the closest radius that is present in table
	// Find the attenuation values that correspond to the radius
	int min_diff = std::numeric_limits<int>::max();
	int closest_key = 0;
	
	for (auto& [key, value] : atten_map) {
		int diff = std::abs(key - l.radius);
		if (diff < min_diff) {
			min_diff = diff;
			closest_key = key;
		}
	}
	l.radius = closest_key;
	pr("Light radius[" << lightIndex << "] set to: " << l.radius << " units");

	glm::vec3 att = atten_map[closest_key];

	l.constant = att.x;
	l.linear = att.y;
	l.quadratic = att.z;
}

void RenderContext::Init() 
{
	std::vector<glm::vec3> lightPos = {
		{ 0. , 5., 0.  },
		{ 15., 5., 0.  },
		{ 0. , 5., 15. },
		{ 15., 5., 15. }
	};

	std::vector<glm::vec3> lightColor = {
		{ 1.f , 1.f , 0.2f },
		{ 0.7f, 1.f , 0.8f },
		{ 1.f , 0.5f, 1.f  },
		{ 0.7f, 1.f , 1.f  }
	};

	std::vector<float> radius = { 20.f, 10.f, 30.f, 5.f };
	std::vector<float> intensity = { 2000.f, 100.f, 30.f, 500.f };

	//float i = 0.1f;

	
	//glm::vec4 ambCol = glm::vec4(amb, amb, amb, 1.f);
	//sceneData = GPUSceneData(ambCol, lightPos, lightColor, radius, intensity);

	

	//ambientColor = ambCol;
	float amb = 0.2f;
	sceneData.ambientColor = glm::vec4(amb, amb, amb, 1.f);

	for (int i = 0; i < MAX_LIGHTS; i++) {
		

		sceneData.lights[i] = {
			.position = lightPos[i],
			.radius = radius[i],

			.color = lightColor[i],

			.ambientFactor = 0.1f,
			.diffuseFactor = 1.0f,
			.specularFactor = 0.5f,
			.intensity = intensity[i],

			//Attenuation skipped. Will be updated based on radius

			.enabled = true
		};

		UpdateLightAttenuation(i);
	}
}
