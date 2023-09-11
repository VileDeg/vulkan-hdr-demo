#include "stdafx.h"

#include "engine.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

void Engine::writeTextureDescriptorSets()
{
	uint32_t diffuseTexBinding = 5;
	uint32_t bumpTexBinding = 6;

	std::vector<VkDescriptorImageInfo> diffuseImageInfos;
	std::vector<VkDescriptorImageInfo> bumpImageInfos;

	for (auto& tex : _diffTexInsertionOrdered) {
		VkDescriptorImageInfo imgInfo = {
			.sampler = _linearSampler,
			.imageView = tex->view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		diffuseImageInfos.push_back(imgInfo);
	}

	for (auto& tex : _bumpTexInsertionOrdered) {
		VkDescriptorImageInfo imgInfo = {
			.sampler = _linearSampler,
			.imageView = tex->view,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		bumpImageInfos.push_back(imgInfo);
	}

	std::vector<VkWriteDescriptorSet> writes;
	VkWriteDescriptorSet writeDiff = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.pNext = nullptr,

		.dstBinding = diffuseTexBinding,

		.descriptorCount = static_cast<uint32_t>(diffuseImageInfos.size()),
		.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo = diffuseImageInfos.data()
	};

	VkWriteDescriptorSet writeBump = writeDiff;
	writeBump.dstBinding = bumpTexBinding;
	writeBump.descriptorCount = bumpImageInfos.size();
	writeBump.pImageInfo = bumpImageInfos.data();

	for (auto& f : _frames) {
		if (!diffuseImageInfos.empty()) {
			writeDiff.dstSet = f.sceneSet;
			writes.push_back(writeDiff);
		}
		if (!bumpImageInfos.empty()) {
			writeBump.dstSet = f.sceneSet;
			writes.push_back(writeBump);
		}
	}

	vkUpdateDescriptorSets(_device, writes.size(), writes.data(), 0, nullptr);
}

void Engine::createScene() //CreateSceneData data
{
	// Reset for when we load a scene at runtime
	cleanupScene();

	// Main model of the scene
	ASSERT(loadModelFromObj("main", MODEL_PATH + _renderContext.modelPath));
	// Sphere model of the light source
	ASSERT(loadModelFromObj("sphere", MODEL_PATH + "sphere/sphere.obj"));
	// Cube model for skybox
	ASSERT(loadModelFromObj("cube", MODEL_PATH + "cube/cube.obj"));

	// Set materials
	for (auto& [key, model] : _models) {
		for (auto& mesh : model.meshes) {
			mesh->material = getMaterial("general");
		}
	}

	writeTextureDescriptorSets();

	loadSkybox(_renderContext.skyboxPath);

	_skyboxObject = std::make_shared<RenderObject>(
		RenderObject{
			.tag = "Skybox",
			.color = {1, 0, 1, 1},
			.model = &_models["cube"],
			.isSkybox = true
		}
	);
	_skyboxObject->model->meshes[0]->material = &_materials["skybox"];

	if (getModel("sphere")) {
		Model* sphr = getModel("sphere");
		// Light source model should not be affected by light
		sphr->lightAffected = false;
		sphr->useObjectColor = true;

		float max_intensity = _renderContext.sceneData.lights[0].intensity;

		for (int i = 0; i < MAX_LIGHTS; i++) {
			auto ptr = std::make_shared<RenderObject>(
				RenderObject{
					.tag = "light" + std::to_string(i),
					.color = glm::vec4(10, 10, 10, 1.),
					.model = sphr,
					.isLightSource = true,
					.pos = _renderContext.sceneData.lights[i].position,
					.scale = glm::vec3(0.1f)
				}
			);

			_renderables.push_back(ptr);
			_renderContext.lightObjects.push_back(ptr);

			max_intensity = std::max(max_intensity, _renderContext.sceneData.lights[i].intensity);
		}

		sphr->meshes[0]->gpuMat.ambientColor *= max_intensity;
		sphr->meshes[0]->gpuMat.diffuseColor *= max_intensity;
		sphr->meshes[0]->gpuMat.specularColor *= max_intensity;

		setDisplayLightSourceObjects(_renderContext.displayLightSourceObjects);
	}

	if (getModel("main")) {
		_renderables.push_back(std::make_shared<RenderObject>(
			RenderObject{
				.tag = "main",
				.model = getModel("main"),
				.pos = _renderContext.modelPos, //glm::vec3(0, -5.f, 0)
				.rot = glm::vec3(0, 90, 0),
				//.scale = glm::vec3(15.f)
				.scale = glm::vec3(_renderContext.modelScale)
			}
		));
		_renderContext.mainObject = _renderables.back();
	}

	_deletionStack.push([this]() {
		cleanupScene();
	});
}


void Engine::loadSkybox(std::string skyboxDirName)
{
	// Partially based on Sascha Willems' cubemap demo: 
	// https://github.com/SaschaWillems/Vulkan/blob/master/examples/texturecubemap/texturecubemap.cpp
	_skyboxDisposeStack.flush();

	constexpr const char* suff[6] = { "px", "nx", "py", "ny", "pz", "nz" };

	std::string basePath = SKYBOX_PATH + skyboxDirName;

	VkDeviceSize imageSize;
	AllocatedBuffer stagingBuffer;

	int baseTexW, baseTexH, baseTexChannels;

	VkFormat imageFormat;
	VkDeviceSize bufferOffset = 0;

	for (int i = 0; i < 6; ++i) {
		std::string path = basePath + "/" + suff[i] + ".hdr";

		void* pixel_ptr = nullptr;
		int texW, texH, texChannels;

		ASSERT(stbi_is_hdr(path.c_str()) != 0);
		float* fpix = stbi_loadf(path.c_str(), &texW, &texH, &texChannels, STBI_rgb_alpha);
		pixel_ptr = fpix;

		imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
		

		ASSERT_MSG(pixel_ptr != nullptr, "Failed to load cubemap face " + std::to_string(i) + ", path: " + path);
		if (i == 0) {
			baseTexW = texW;
			baseTexH = texH;
			baseTexChannels = texChannels;

			imageSize = (VkDeviceSize)baseTexW * (VkDeviceSize)baseTexH * 4 * 4;

			VkDeviceSize bufferSize = imageSize * 6;
			// Allocate temporary buffer for holding texture data to upload
			stagingBuffer = allocateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
		} else {
			// Make sure all the faces of cubemap have exactly the same dimensions and color channels
			ASSERT(baseTexW == texW && baseTexH == texH && baseTexChannels == texChannels);
		}

		// Copy data to buffer
		void* dst = (char*)stagingBuffer.gpu_ptr + bufferOffset;
		memcpy(dst, pixel_ptr, static_cast<size_t>(imageSize));
	
		// We no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
		stbi_image_free(pixel_ptr);

		bufferOffset += imageSize;
	}

	VkImageSubresourceRange subresourceRange = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 6
	};

	// Create optimal tiled target image
	VkImageCreateInfo imageInfo{
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		// This flag is required for cube map images
		.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = imageFormat,
		.extent = {
			.width = static_cast<uint32_t>(baseTexW),
			.height = static_cast<uint32_t>(baseTexH),
			.depth = 1
		},
		.mipLevels = 1,
		// Cube faces count as array layers in Vulkan
		.arrayLayers = 6,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	// This creates entry in cache
	//_skybox = _textures[basePath];
	_skybox.tag = basePath;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//allocate and create the image
	VK_ASSERT(vmaCreateImage(_allocator, &imageInfo, &dimg_allocinfo, &_skybox.allocImage.image, &_skybox.allocImage.allocation, nullptr));

	immediate_submit(
		[&](VkCommandBuffer cmd) {
			// Setup buffer copy regions for each face including all of its miplevels
			std::vector<VkBufferImageCopy> bufferCopyRegions;
			VkDeviceSize offset = 0;

			for (uint32_t face = 0; face < 6; face++)
			{
				// Calculate offset into staging buffer for the current mip level and face
				VkBufferImageCopy bufferCopyRegion = {
					.bufferOffset = offset,
					.imageSubresource = {
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.mipLevel = 0,
						.baseArrayLayer = face,
						.layerCount = 1,
					},
					.imageExtent = {
						.width = static_cast<uint32_t>(baseTexW),
						.height = static_cast<uint32_t>(baseTexH),
						.depth = 1,
					}
				};

				bufferCopyRegions.push_back(bufferCopyRegion);
				offset += imageSize;
			}

			vk_utils::imageMemoryBarrier(cmd, _skybox.allocImage.image,
				0, 
				VK_ACCESS_TRANSFER_WRITE_BIT,

				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				subresourceRange);

			//copy the buffer into the image
			vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, _skybox.allocImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

			vk_utils::imageMemoryBarrier(cmd, _skybox.allocImage.image,
				VK_ACCESS_TRANSFER_WRITE_BIT, 
				VK_ACCESS_SHADER_READ_BIT,

				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				subresourceRange);
		}
	);

	// Create image view
	VkImageViewCreateInfo imageinfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = _skybox.allocImage.image,
		.viewType = VK_IMAGE_VIEW_TYPE_CUBE,
		.format = imageFormat,
		.subresourceRange = subresourceRange,
	};

	VK_ASSERT(vkCreateImageView(_device, &imageinfo, nullptr, &_skybox.view));

	stagingBuffer.destroy(_allocator);
	pr("Cubemap loaded successfully: " << basePath);

	// write cubemap to descriptor set
	_skybox.allocImage.descInfo = {
		.sampler = _linearSampler,//cubemapSampler,
		.imageView = _skybox.view,
		.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	};

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VkWriteDescriptorSet skyboxWrite =
			vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 
				_frames[i].sceneSet, &_skybox.allocImage.descInfo, 3);
		vkUpdateDescriptorSets(_device, 1, &skyboxWrite, 0, nullptr);
	}

	_skyboxDisposeStack.push([=]() mutable {
		vkDestroyImageView(_device, _skybox.view, nullptr);
		vmaDestroyImage(_allocator, _skybox.allocImage.image, _skybox.allocImage.allocation);
	});

	_sceneDisposeStack.push([=]() mutable {
		_skyboxDisposeStack.flush();
	});
}



Attachment* Engine::loadTextureFromFile(const char* path)
{
	/* Based on https://github.com/vblanco20-1/vulkan-guide */

	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (!pixels) {
		PRERR("Failed to load texture file " << path);
		return nullptr;
	}

	void* pixel_ptr = pixels;
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	// The format R8G8B8A8 matches exactly with the pixels loaded from stb_image lib
	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

	// Allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer = allocateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	// Copy data to buffer
	memcpy(stagingBuffer.gpu_ptr, pixel_ptr, static_cast<size_t>(imageSize));
	
	// We no longer need the loaded data, so we can free the pixels as they are now in the staging buffer
	stbi_image_free(pixels);

	VkExtent3D imageExtent{
		.width = static_cast<uint32_t>(texWidth),
		.height = static_cast<uint32_t>(texHeight),
		.depth = 1
	};

	VkImageSubresourceRange subresourceRange = {
		.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
		.baseMipLevel = 0,
		.levelCount = 1,
		.baseArrayLayer = 0,
		.layerCount = 1
	};

	VkImageCreateInfo dimg_info =
		vkinit::image_create_info(imageFormat,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent);

	// This creates entry in cache
	Attachment& newTexture = _textures[path];
	newTexture.tag = path;

	VmaAllocationCreateInfo dimg_allocinfo = {};
	dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//allocate and create the image
	vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &newTexture.allocImage.image, &newTexture.allocImage.allocation, nullptr);

	immediate_submit(
		[&](VkCommandBuffer cmd) {
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

			vk_utils::imageMemoryBarrier(cmd, newTexture.allocImage.image,
				0,
				VK_ACCESS_TRANSFER_WRITE_BIT,

				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,
				subresourceRange);

			//copy the buffer into the image
			vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newTexture.allocImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

			vk_utils::imageMemoryBarrier(cmd, newTexture.allocImage.image,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,

				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				subresourceRange);
		}
	);

	VkImageViewCreateInfo imageinfo =
		vkinit::imageview_create_info(VK_FORMAT_R8G8B8A8_SRGB, newTexture.allocImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

	VK_ASSERT(vkCreateImageView(_device, &imageinfo, nullptr, &newTexture.view));

	_sceneDisposeStack.push([=]() mutable {
		vkDestroyImageView(_device, newTexture.view, nullptr);
		vmaDestroyImage(_allocator, newTexture.allocImage.image, newTexture.allocImage.allocation);
	});

	stagingBuffer.destroy(_allocator);
	pr("\n\tTexture loaded successfully: " << path);

	

	return &_textures[path];
}



void Engine::createMeshBuffer(Mesh& mesh, bool isVertexBuffer)
{
	ASSERT(isVertexBuffer && mesh.vertices.size() > 0 || !isVertexBuffer && mesh.indices.size() > 0);

	const size_t bufferSize = isVertexBuffer ?
		mesh.vertices.size() * sizeof(Vertex) :
		mesh.indices.size() * sizeof(uint32_t);

	// Allocate temporary buffer for holding texture data to upload
	AllocatedBuffer stagingBuffer = allocateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

	if (isVertexBuffer) {
		memcpy(stagingBuffer.gpu_ptr, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
	} else {
		memcpy(stagingBuffer.gpu_ptr, mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
	}

	VkBufferUsageFlags usg = (isVertexBuffer ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

	AllocatedBuffer& allocBuffer = isVertexBuffer ? mesh.vertexBuffer : mesh.indexBuffer;

	allocBuffer = allocateBuffer(bufferSize, usg | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

	immediate_submit([&](VkCommandBuffer cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer.buffer, allocBuffer.buffer, 1, &copy);
		});

	_sceneDisposeStack.push([&]() {
		allocBuffer.destroy(_allocator);
		});

	// Destroy staging buffer now as we don't need it anymore
	stagingBuffer.destroy(_allocator);
}

void Engine::uploadMesh(Mesh& mesh)
{
	// Create vertex buffer
	createMeshBuffer(mesh, true);
	// Create index buffer
	createMeshBuffer(mesh, false);
}



void Engine::loadScene(std::string sceneFullPath)
{
	std::string scene_name = "dobrovic-sponza";
	std::ifstream in(sceneFullPath);
	ASSERT(in.good());

	nlohmann::json j;
	j << in;

	auto& mp = j["model_position"];
	//CreateSceneData data = {
	//	.modelPos = { mp[0], mp[1], mp[2] },
	//	.modelScale = j["model_scale"],
	//	/*.bumpStrength = j["bump_strength"],*/
	//	.modelPath = j["model_name"],
	//	.skyboxPath = j["skybox"]
	//};
	auto& rc = _renderContext;
	rc.modelPos = { mp[0], mp[1], mp[2] };
	rc.modelScale = j["model_scale"];		
	rc.modelPath = j["model_name"];
	rc.skyboxPath = j["skybox"];
	

	rc.sceneData.bumpStrength = j["bump_strength"];

	for (int i = 0; i < MAX_LIGHTS; ++i) {
		auto& json_l = j["lights"][i];
		auto& rc_l = rc.sceneData.lights[i];
		auto p = json_l["position"];

		rc_l.enabled = json_l["enabled"];
		rc_l.intensity = json_l["intensity"];
		rc_l.position = { p[0], p[1], p[2] };
		/*data.intensity[i] = l[i]["intensity"];
		auto p = l[i]["position"];
		data.position[i].x = p[0];
		data.position[i].y = p[1];
		data.position[i].z = p[2];*/
	}

	createScene();
}

void Engine::saveScene(std::string sceneFullPath)
{
	auto& rc = _renderContext;
	auto& sd = rc.sceneData;

	nlohmann::json j;
	j["model_name"] = rc.modelPath;

	glm::vec3 mp = rc.mainObject->pos;

	j["model_position"] = { mp.x, mp.y, mp.z };
	j["model_scale"] = rc.modelScale;
	j["bump_strength"] = sd.bumpStrength;

	auto arr = nlohmann::json::array();

	for (int i = 0; i < MAX_LIGHTS; ++i) {
		auto& l = sd.lights[i];
		auto& p = l.position;

		auto arr1 = nlohmann::json::object();
		arr1["enabled"] = (bool)l.enabled;
		arr1["position"] = { p.x, p.y, p.z };
		arr1["intensity"] = l.intensity;

		arr.push_back(arr1);
	}

	j["lights"] = arr;
	j["skybox"] = rc.skyboxPath;

	std::ofstream out(sceneFullPath);
	ASSERT(out.good());

	out << std::setw(2) << j;
}


void Engine::createSamplers() {
	// Create samplers for textures
	VkSamplerCreateInfo samplerInfo = vkinit::sampler_create_info(VK_FILTER_LINEAR);
	VkSamplerCreateInfo samplerInfo1 = vkinit::sampler_create_info(VK_FILTER_NEAREST);

	VK_ASSERT(vkCreateSampler(_device, &samplerInfo, nullptr, &_linearSampler));
	VK_ASSERT(vkCreateSampler(_device, &samplerInfo1, nullptr, &_nearestSampler));

	_deletionStack.push([=]() {
		vkDestroySampler(_device, _linearSampler, nullptr);
		vkDestroySampler(_device, _nearestSampler, nullptr);
	});
}

void Engine::cleanupScene()
{
	vkDeviceWaitIdle(_device);

	_renderables.clear();
	_renderContext.lightObjects.clear();
	_models.clear();

	_sceneDisposeStack.flush();

	_meshes.clear();
	_textures.clear();

	modelLoaderGlobalDiffuseTexIndex = 0;
	modelLoaderGlobalBumpTexIndex = 0;
	_diffTexInsertionOrdered.clear();
	_bumpTexInsertionOrdered.clear();
}

