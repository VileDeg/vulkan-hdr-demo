#include "stdafx.h"

#include "resources.h"
#include "Engine.h"

#include "tinyobj/tiny_obj_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

static std::string GetBaseDir(const std::string& filepath) {
	//From https://github.com/tinyobjloader/tinyobjloader
	if (filepath.find_last_of("/\\") != std::string::npos)
		return filepath.substr(0, filepath.find_last_of("/\\"));
	return "";
}

static bool FileExists(const std::string& abs_filename) {
	//From https://github.com/tinyobjloader/tinyobjloader
	bool ret;
	FILE* fp = fopen(abs_filename.c_str(), "rb");
	if (fp) {
		ret = true;
		fclose(fp);
	} else {
		ret = false;
	}

	return ret;
}

static std::string GetFileNameNoExt(const std::string& filepath) {
	std::string subs = filepath;
	auto dotp = subs.find_last_of(".");
	auto slashp = subs.find_last_of("/\\");

	if (dotp != std::string::npos) {
		subs = subs.substr(0, dotp);
	}
	if (slashp != std::string::npos) {
		subs = subs.substr(slashp, subs.length()-1-slashp);
	}

	return subs;
}


void Engine::UpdateSSBOData(const std::vector<std::shared_ptr<RenderObject>>& objects)
{
	// Following is the maximum pixel value of image that is used for exposure simulation
	auto& sd = _renderContext.ssboData;

	sd.exposureON = _inp.exposureEnabled;
	sd.toneMappingON = _inp.toneMappingEnabled;

	for (int i = 0; i < objects.size(); i++) {
		sd.objects[i].modelMatrix = objects[i]->Transform();
		sd.objects[i].color = objects[i]->color;
	}
}

void Engine::drawObjects(VkCommandBuffer cmd, const std::vector<std::shared_ptr<RenderObject>>& objects)
{
	{ // Load SSBO to GPU
		UpdateSSBOData(objects);
		getCurrentFrame().objectBuffer.runOnMemoryMap(_allocator, 
			[&](void* data) {
				char* ssboData = (char*)data;
				unsigned int* newMax = (unsigned int*)(ssboData + 16);
				unsigned int* oldMax = (unsigned int*)(ssboData + 16 + 4);
				float* f_oldMax = reinterpret_cast<float*>(oldMax);

				if (_frameNumber == 0) { // Initially new MAX and old MAX are zero.
					*newMax = 0;
					*oldMax = 0;
				} else { // On every next frame, swap new MAX and old MAX and set new MAX to zero.
					std::swap(*newMax, *oldMax);
					*newMax = 0;
				}
				auto& sd = _renderContext.ssboData;
				sd.newMax = *newMax;
				sd.oldMax = *oldMax;

				memcpy(data, &sd, sizeof(GPUSSBOData));
			}
		);
	}

	{ // Load UNIFORM BUFFER of scene parameters to GPU
		_renderContext.sceneData.cameraPos = _inp.camera.GetPos();
		_sceneParameterBuffer.runOnMemoryMap(_allocator, 
			[&](void* data) {
				char* sceneData = (char*)data;
				sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;
				memcpy(sceneData, &_renderContext.sceneData, sizeof(GPUSceneData));
			}
		);
	}
	
	glm::mat4 viewMat = _inp.camera.GetViewMat();
	glm::mat4 projMat = _inp.camera.GetProjMat(_fovY, _windowExtent.width, _windowExtent.height);
	GPUCameraData camData{
		.view = viewMat,
		.proj = projMat,
		.viewproj = projMat * viewMat,
	};
	getCurrentFrame().cameraBuffer.runOnMemoryMap(_allocator, 
		[&](void* data) {
			memcpy(data, &camData, sizeof(GPUCameraData));
		}
	);

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < objects.size(); i++) {
        const RenderObject& obj = *objects[i];

		// If the material is different, bind the new material
		if (obj.material != lastMaterial) {
			bindPipeline(cmd, obj.material->pipeline);
			lastMaterial = obj.material;

			//offset for our scene buffer
			uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;

			// Always add the global and object descriptor
			std::vector<VkDescriptorSet> sets = { getCurrentFrame().globalDescriptor, getCurrentFrame().objectDescriptor };
			// If the material has a texture, add texture descriptor
			if (obj.material->textureSet != VK_NULL_HANDLE) {
				sets.push_back(obj.material->textureSet);
            }
			// Bind the descriptor sets
			vkCmdBindDescriptorSets(
				cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, obj.material->pipelineLayout, 0, sets.size(), sets.data(), 1, &uniform_offset);
		}

		if (obj.mesh != lastMesh) {
			VkDeviceSize zeroOffset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &obj.mesh->_vertexBuffer.buffer, &zeroOffset);
			lastMesh = obj.mesh;
		}
		
		ASSERT(obj.material && obj.mesh);

		// Ve send loop index as instance index to use it in shader to access object data in SSBO
        vkCmdDraw(cmd, obj.mesh->_vertices.size(), 1, 0, i);
    }
}

void Engine::createScene()
{
	ASSERT(loadTextureFromFile("lostEmpire", Engine::imagePath + "lost_empire/lost_empire-RGBA.png") != nullptr);

	ASSERT(loadModelFromObj("ball" , Engine::modelPath + "CustomBall/CustomBall.obj"));
	ASSERT(loadModelFromObj("model", Engine::modelPath + "lost_empire/lost_empire.obj"));


	_renderContext.Init();

	for (int i = 0; i < MAX_LIGHTS; i++) {
		_renderables.push_back(std::make_shared<RenderObject>(
			RenderObject{
				.tag = "light" + std::to_string(i),
				//.color = _renderContext.lightColor[i] * _renderContext.intensity[i],
				.color = glm::vec4(0.5, 0.5, 0.5, 1.),
				.mesh = getMesh("ball"),
				.material = getMaterial("colored"),
				.pos = _renderContext.sceneData.lights[i].position,
				.scale = glm::vec3(10.f)
			}
		));

		_renderContext.lightObjects.push_back(_renderables.back());
    }

	Material* mat = getMaterial("textured");

	_renderables.push_back(std::make_shared<RenderObject>(
		RenderObject{
			.mesh = getMesh("model"),
			.material = mat,
			.pos = glm::vec3(0, -18.f, 0)
		}
	));

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
		.imageView = getTexture("lostEmpire")->imageView,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	};

	VkWriteDescriptorSet texture1 = 
		vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
			mat->textureSet, &imageBufferInfo, 0);

	vkUpdateDescriptorSets(_device, 1, &texture1, 0, nullptr);
}

bool Engine::loadModelFromObj(const std::string assignedName, const std::string path)
{
	Mesh& newMesh = _meshes[assignedName];
	
	//Model newModel;

	//From https://github.com/tinyobjloader/tinyobjloader
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	float bmin[3], bmax[3];

	std::string baseDir = GetBaseDir(path);
	if (baseDir.empty()) {
		baseDir = ".";
	}
#ifdef _WIN32
	baseDir += "\\";
#else
	baseDir += "/";
#endif

	pr("Loading model at: " << path);
	bool good = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), baseDir.c_str());

	if (!warn.empty()) {
		pr("\ttinyobj: WARN: " << warn);
	}
	if (!err.empty()) {
		pr("\ttinyobj: ERR: " << err);
	}
	if (!good || !err.empty()) {
		pr("\ttinyobj: Failed to load " << path);
		return false;
	}

	// Materials
	materials.push_back(tinyobj::material_t());

	for (size_t i = 0; i < materials.size(); i++) {
		pr("\tmaterial[" << i << "].diffuse_texname = " << materials[i].diffuse_texname);
	}

	// Load diffuse textures
#if 0
	{
		for (size_t m = 0; m < materials.size(); m++) {
			tinyobj::material_t* mp = &materials[m];

			if (mp->diffuse_texname.length() > 0) {
				// Only load the texture if it is not already loaded
				if (getTexture(mp->diffuse_texname) == nullptr) {
					unsigned int texture_id;
					int w, h;
					int comp;

					std::string texture_filename = mp->diffuse_texname;
					if (!FileExists(texture_filename)) {
						// Append base dir.
						texture_filename = baseDir + mp->diffuse_texname;
						if (!FileExists(texture_filename)) {
							PRERR("Unable to find file: " << mp->diffuse_texname);
							EXIT(1);
						}
					}


					Texture* texture;
					if (!(texture = loadTextureFromFile(mp->diffuse_texname, texture_filename))) {
						PRERR("Unable to load texture: " << texture_filename);
						EXIT(1);
					}

					newModel.textures.push_back(texture);
				}
			}
		}
	}
#endif

	// Loop over shapes
	for (size_t s = 0; s < shapes.size(); s++) {
		pr("\ttinyobj: Loading shape[" << s << "]: " << shapes[s].name);

		// Create mesh cache
		//Mesh& newMesh = _meshes["Mesh"+std::to_string(s)];

		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {

			ASSERT(shapes[s].mesh.num_face_vertices[f] == 3);
			//hardcode loading to triangles
			int fv = 3;

			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {
				// access to vertex
				tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

				//vertex position
				tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
				tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
				tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];

				//vertex normal
				tinyobj::real_t nx{ 0 }, ny{ 0 }, nz{ 0 };
				if (idx.normal_index >= 0) {
					nx = attrib.normals[3 * idx.normal_index + 0];
					ny = attrib.normals[3 * idx.normal_index + 1];
					nz = attrib.normals[3 * idx.normal_index + 2];
				}
				//vertex uv
				tinyobj::real_t ux{ 0 }, uy{ 0 };
				if (idx.texcoord_index >= 0) {
					ux = attrib.texcoords[2 * idx.texcoord_index + 0];
					uy = attrib.texcoords[2 * idx.texcoord_index + 1];
				}

				//copy it into our vertex
				Vertex new_vert{
					.pos = {vx, vy, vz},
					.normal = {nx, ny, nz},
					//we are setting the vertex color as the vertex normal. This is just for display purposess
					.color = {nx, ny, nz},
					.uv = {ux, 1 - uy}
				};

				newMesh._vertices.push_back(new_vert);
			}
			index_offset += fv;

			// per-face material
			//shapes[s].mesh.material_ids[f];
		}
		/*uploadMesh(newMesh);
		newModel.meshes.push_back(&newMesh);*/
	}
	uploadMesh(newMesh);
	//_models[GetFileNameNoExt(path)] = newModel;

	return true;
}

Texture* Engine::loadTextureFromFile(const std::string assignedName, const std::string path)
{
	/* Based on https://github.com/vblanco20-1/vulkan-guide */
	int texWidth, texHeight, texChannels;

	stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha); // STBI_default

	if (!pixels) {
		PRERR("Failed to load texture file " << path);
		return nullptr;
	}

	void* pixel_ptr = pixels;
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	//the format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
	VkFormat image_format = VK_FORMAT_R8G8B8A8_SRGB;

	//allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer = createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	//copy data to buffer
	stagingBuffer.runOnMemoryMap(_allocator, [&](void* data) {
		memcpy(data, pixel_ptr, static_cast<size_t>(imageSize));
	});


	//we no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
	stbi_image_free(pixels);

	VkExtent3D imageExtent{
		.width = static_cast<uint32_t>(texWidth),
		.height = static_cast<uint32_t>(texHeight),
		.depth = 1
	};

	VkImageCreateInfo dimg_info = 
		vkinit::image_create_info(image_format, 
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

	
	Texture newTexture;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &newTexture.image.image, &newTexture.image.allocation, nullptr);

	immediate_submit(
		[&](VkCommandBuffer cmd) {
			VkImageMemoryBarrier imageBarrier_toTransfer{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				.image = newTexture.image.image,
				.subresourceRange = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			};

			//barrier the image into the transfer-receive layout
			vkCmdPipelineBarrier(cmd,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toTransfer);

			VkBufferImageCopy copyRegion{
				.bufferOffset = 0,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.mipLevel = 0,
					.baseArrayLayer = 0,
					.layerCount = 1,
				},
				.imageExtent = imageExtent
			};

			//copy the buffer into the image
			vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newTexture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			VkImageMemoryBarrier imageBarrier_toReadable = imageBarrier_toTransfer;

			imageBarrier_toReadable.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier_toReadable.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			imageBarrier_toReadable.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier_toReadable.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			//barrier the image into the shader readable layout
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier_toReadable);
		}
	);


	VkImageViewCreateInfo imageinfo = 
		vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, newTexture.image.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VKASSERT(vkCreateImageView(_device, &imageinfo, nullptr, &newTexture.imageView));


	_textures[assignedName] = newTexture;

	_deletionStack.push([=]() mutable {
		vkDestroyImageView(_device, newTexture.imageView, nullptr);
		newTexture.image.destroy(_allocator);
		});

	stagingBuffer.destroy(_allocator);
	pr("Texture loaded successfully: " << path);

	return &_textures[assignedName];
}

void Engine::uploadMesh(Mesh& mesh)
{
	//mesh.initBuffers(_allocator);
	const size_t bufferSize = mesh.getBufferSize();

	VkBufferCreateInfo stagingBufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = sizeof(Vertex) * mesh._vertices.size(),
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	VmaAllocationCreateInfo allocInfo = {
		.usage = VMA_MEMORY_USAGE_CPU_ONLY,
	};
	//allocate temporary buffer for holding texture data to upload

	AllocatedBuffer stagingBuffer;
	VKASSERT(vmaCreateBuffer(_allocator, &stagingBufferInfo, &allocInfo,
		&stagingBuffer.buffer, &stagingBuffer.allocation, nullptr));

	void* data;
	vmaMapMemory(_allocator, stagingBuffer.allocation, &data);
	memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(_allocator, stagingBuffer.allocation);

	//allocate vertex buffer
	VkBufferCreateInfo vertexBufferInfo = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		//this is the total size, in bytes, of the buffer we are allocating
		.size = mesh.getBufferSize(),
		//this buffer is going to be used as a Vertex Buffer
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
	};

	//let the VMA library know that this data should be GPU native
	VmaAllocationCreateInfo vmaAllocInfo = {
		.usage = VMA_MEMORY_USAGE_GPU_ONLY,
	};

	//allocate the buffer
	VKASSERT(vmaCreateBuffer(_allocator, &vertexBufferInfo, &vmaAllocInfo,
		&mesh._vertexBuffer.buffer,
		&mesh._vertexBuffer.allocation,
		nullptr));

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = mesh.getBufferSize();
		vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh._vertexBuffer.buffer, 1, &copy);
		});

	_deletionStack.push([&]() {
		mesh._vertexBuffer.destroy(_allocator);
		});

	// Destroy staging buffer now as we don't need it anymore
	stagingBuffer.destroy(_allocator);
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
	Light& l = sceneData.lights[lightIndex];

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
	std::vector<glm::vec3> lightPos = {
		{ 0. , 5., 0.  },
		{ 15., 5., 0.  },
		{ 0. , 5., 15. },
		{ 15., 5., 15. }
	};

	std::vector<glm::vec3> lightColor = {
		{ 1.f , 1.f , 1.f },
		{ 1.f , 1.f , 1.f },
		{ 1.f , 1.f , 1.f },
		{ 1.f , 1.f , 1.f }
	};

	std::vector<float> radius = { 20.f, 10.f, 30.f, 5.f };
	std::vector<float> intensity = { 2000.f, 100.f, 30.f, 500.f };
	//std::vector<float> intensity = { 1.f, 1.f, 1.f, 1.f };

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

		UpdateLightAttenuation(i, 0);
	}
}



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

Texture* Engine::getTexture(const std::string& name)
{
	auto it = _textures.find(name);
	if (it == _textures.end()) {
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
