#include "stdafx.h"
#include "engine.h"

#include "imgui/imgui.h"

static constexpr int ACCESS_MASK_ALL =
	VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
	VK_ACCESS_INDEX_READ_BIT |
	VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
	VK_ACCESS_UNIFORM_READ_BIT |
	VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
	VK_ACCESS_SHADER_READ_BIT |
	VK_ACCESS_SHADER_WRITE_BIT |
	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
	VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
	VK_ACCESS_TRANSFER_READ_BIT |
	VK_ACCESS_TRANSFER_WRITE_BIT |
	VK_ACCESS_HOST_READ_BIT |
	VK_ACCESS_HOST_WRITE_BIT;



static void cmdSetViewportScissor(VkCommandBuffer cmd, uint32_t w, uint32_t h) 
{
	VkViewport viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(w),
			.height = static_cast<float>(h),
			.minDepth = 0.0f,
			.maxDepth = 1.0f
	};

	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D scissor{
		.offset = { 0, 0 },
		.extent = { w, h }
	};

	vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void Engine::drawObject(VkCommandBuffer cmd, const std::shared_ptr<RenderObject>& object, Material** lastMaterial, Mesh** lastMesh, uint32_t index) {
	const RenderObject& obj = *object;
	Model* model = obj.model;

	for (uint32_t m = 0; m < model->meshes.size(); ++m) {
		Mesh* mesh = model->meshes[m];

		ASSERT(mesh && mesh->material);

		// Always add the sceneData and SSBO descriptor
		std::vector<VkDescriptorSet> sets = { _frames[_frameInFlightNum].globalSet };

		{ // Push diffuse texture descriptor set
			constexpr int txc = 2;
			std::array<VkImageView, txc> imageView = { VK_NULL_HANDLE, VK_NULL_HANDLE };
			if (!obj.isSkybox) {
				if (mesh->diffuseTex != nullptr) {
					imageView[0] = mesh->diffuseTex->view;
				}
				if (mesh->bumpTex != nullptr) {
					imageView[1] = mesh->bumpTex->view;
				}
			}

			std::array<VkDescriptorImageInfo, txc> imageInfo;
			std::array<VkWriteDescriptorSet, txc>  descWrites;

			for (int i = 0; i < txc; ++i) {
				imageInfo[i] = {
					.sampler = _linearSampler,
					.imageView = imageView[i],
					.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				};

				descWrites[i] = vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					VK_NULL_HANDLE, &imageInfo[i], i);
			}

			vkCmdPushDescriptorSetKHR(
				cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipelineLayout, 1, txc, descWrites.data());
		}

		// Load push constants to GPU
		GPUScenePC pc = {
			.lightAffected = model->lightAffected,
			.isCubemap = obj.isSkybox,
			.objectIndex = index,
			.meshIndex = m,
			.useDiffTex = (mesh->diffuseTex!= nullptr),
			.useBumpTex = (mesh->bumpTex != nullptr)
		};

		vkCmdPushConstants(
			cmd, mesh->material->pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUScenePC),
			&pc);

		// If the material is different, bind the new material
		if (mesh->material != *lastMaterial) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipeline);
			// Bind the descriptor sets
			vkCmdBindDescriptorSets(
				cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipelineLayout, 0, sets.size(), sets.data(), 0, nullptr);

			*lastMaterial = mesh->material;
		}

		if (mesh != *lastMesh) {
			VkDeviceSize zeroOffset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer.buffer, &zeroOffset);
			vkCmdBindIndexBuffer(cmd, mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			*lastMesh = mesh;
		}

		// Ve send loop index as instance index to use it in shader to access object data in SSBO
		vkCmdDrawIndexed(cmd, mesh->indices.size(), 1, 0, 0, 0); // index
	}
}

void Engine::drawObjects(VkCommandBuffer cmd, const std::vector<std::shared_ptr<RenderObject>>& objects)
{
	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (uint32_t i = 0; i < objects.size(); i++) {
		drawObject(cmd, objects[i], &lastMaterial, &lastMesh, i);
	}

	if (_renderContext.enableSkybox) {
		// Draw skybox as the last object
		drawObject(cmd, _skyboxObject, &lastMaterial, &lastMesh, 0);
	}
}

void Engine::updateCubeFace(FrameData& f, uint32_t lightIndex, uint32_t faceIndex)
{
	VkImageSubresourceRange range{};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.levelCount = VK_REMAINING_MIP_LEVELS;
	range.baseArrayLayer = 0;
	range.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkImageSubresourceRange depth_range{ range };
	depth_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

	// Translate to required layout
	utils::imageMemoryBarrier(f.cmd, _shadow.cubemapArray.allocImage.image,
		0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

		range);


	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderingAttachmentInfoKHR color_attachment_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
		.imageView = _shadow.faceViews[lightIndex][faceIndex],
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.resolveMode = VK_RESOLVE_MODE_NONE,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clearValues[0]
	};

	VkRenderingAttachmentInfoKHR depth_attachment_info = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
		.imageView = _shadow.depth.view,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.resolveMode = VK_RESOLVE_MODE_NONE,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clearValues[1]
	};

	VkRect2D area = {
		.extent = { _shadow.width, _shadow.height }
	};
	
	VkRenderingInfoKHR renderingInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
		.renderArea = area,
		.layerCount = 1,
		.colorAttachmentCount = 1,
		.pColorAttachments = &color_attachment_info,
		.pDepthAttachment = &depth_attachment_info
	};

	vkCmdBeginRendering(f.cmd, &renderingInfo);
	{
		GPUShadowPC pc = {
			.view = _renderContext.lightView[faceIndex],
			.far_plane = _renderContext.zFar,
			.lightIndex = lightIndex
		};

		Material& mat = _materials["shadow"];
		// Update shader push constant block
		// Contains current face view matrix
		vkCmdPushConstants(
			f.cmd,
			mat.pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT,
			0,
			sizeof(GPUShadowPC),
			&pc);

		{ // Draw all objects' shadows
			for (int i = 0; i < _renderables.size(); ++i) {
				const RenderObject& obj = *_renderables[i];
				Model* model = obj.model;
				if (model == nullptr || !model->lightAffected) {
					continue;
				}

				for (int m = 0; m < model->meshes.size(); ++m) {
					Mesh* mesh = model->meshes[m];

					VkDeviceSize zeroOffset = 0;
					vkCmdBindVertexBuffers(f.cmd, 0, 1, &mesh->vertexBuffer.buffer, &zeroOffset);

					vkCmdBindIndexBuffer(f.cmd, mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

					// We send loop index as instance index to use it in shader to access object data in SSBO
					vkCmdDrawIndexed(f.cmd, mesh->indices.size(), 1, 0, 0, i);
				}
			}
		}
	}
	vkCmdEndRendering(f.cmd);

	// Translate to back to optimal layout for sampling
	utils::imageMemoryBarrier(f.cmd, _shadow.cubemapArray.allocImage.image,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,

		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

		range);
}

void Engine::loadDataToGPU()
{
	auto& objects = _renderables;
	// Load SSBO to GPU
	{
		for (int i = 0; i < objects.size(); i++) {
			glm::mat4 modelMat = objects[i]->Transform();
			_gpu.ssbo->objects[i] = {
				.modelMatrix = modelMat,
				.normalMatrix = glm::mat3(glm::transpose(glm::inverse(modelMat)))
				//.color = objects[i]->color
			};

			for (int m = 0; m < objects[i]->model->meshes.size(); ++m) {
				_gpu.ssbo->objects[i].mat[m] = objects[i]->model->meshes[m]->gpuMat;
			}
		}
	}

	// Load UNIFORM BUFFER of scene parameters to GPU
	{
		/*char* sd = (char*)_sceneParameterBuffer.gpu_ptr;
		sd += pad_uniform_buffer_size(sizeof(GPUSceneUB)) * _frameInFlightNum;*/

		_renderContext.sceneData.cameraPos = _camera.GetPos();
		//flag(_renderContext.sceneData.showShadowMap);
		_renderContext.sceneData.lightFarPlane = _renderContext.zFar;
		//_renderContext.sceneData.showShadowMap = false;

		memcpy(_gpu.scene, &_renderContext.sceneData, sizeof(GPUSceneUB));
	}

	{ // Load UNIFORM BUFFER of camera to GPU
		glm::mat4 viewMat = _camera.GetViewMat();
		glm::mat4 projMat = _camera.GetProjMat(_fovY, _viewport.imageExtent.width, _viewport.imageExtent.height);

		*_gpu.camera = {
			.view = viewMat,
			.proj = projMat,
			.viewproj = projMat * viewMat,
		};
	}

	{ // Load compute SSBO to GPU
		using uint = unsigned int;
		uint sum = 0;
		//uint tmp_sum = 0;
		uint li = 0;
		uint ui = MAX_LUMINANCE_BINS - 1;
		float lb = _renderContext.lumPixelLowerBound * _renderContext.comp.totalPixelNum;
		float ub = _renderContext.lumPixelUpperBound * _renderContext.comp.totalPixelNum;

		//uint sum_skipped = 0;

		for (uint i = 0; i < MAX_LUMINANCE_BINS; ++i) {
			//tmp_sum += _gpu.compSSBO->luminance[i];

			sum += _gpu.compSSBO->luminance[i];
			if (sum > lb) {
				li = i;
				break;
			}
		}

		//sum_skipped += sum;

		for (uint i = li + 1; i < MAX_LUMINANCE_BINS; ++i) {
			//tmp_sum += _gpu.compSSBO->luminance[i];
			sum += _gpu.compSSBO->luminance[i];
			if (sum > ub) {
				ui = i;
				break;
			}
		}

		//sum_skipped += _renderContext.comp.totalPixelNum - sum;

		_renderContext.comp.logLumRange = _renderContext.maxLogLuminance - _renderContext.comp.minLogLum;
		_renderContext.comp.oneOverLogLumRange = 1.f / _renderContext.comp.logLumRange;
		_renderContext.comp.totalPixelNum = _viewport.imageExtent.width * _viewport.imageExtent.height;
		_renderContext.comp.timeCoeff = 1 - std::exp(-_deltaTime * _renderContext.eyeAdaptationTimeCoefficient);
		_renderContext.comp.lumLowerIndex = li;
		_renderContext.comp.lumUpperIndex = ui;
		float avg = (_viewport.imageExtent.width + _viewport.imageExtent.height) / 2;
		_renderContext.comp.sigmaS = avg * 0.02; //Set spatial sigma to equal 2% of viewport size

		memcpy(_gpu.compUB, &_renderContext.comp, sizeof(GPUCompUB));

		// Reset luminance from previous frame
		memset(_gpu.compSSBO->luminance, 0, sizeof(_gpu.compSSBO->luminance));
	}
}


static void s_fullBarrier(VkCommandBuffer& cmd) {
	VkMemoryBarrier memoryBarrier = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
		.srcAccessMask = ACCESS_MASK_ALL,
		.dstAccessMask = ACCESS_MASK_ALL
	};

	vkCmdPipelineBarrier(
		cmd,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // srcStageMask
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, // dstStageMask
		0,
		1,                                  // memoryBarrierCount
		&memoryBarrier,                     // pMemoryBarriers
		0, 0, 0, 0
	);
}

static void generateMips(VkCommandBuffer& cmd, VkImage& image, int numOfMips, uint32_t width, uint32_t height) 
{
	int32_t w = width;
	int32_t h = height;

	VkImageSubresourceRange srcRange = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = 1,
			.baseArrayLayer = 0,
			.layerCount = 1
	};
	VkImageSubresourceRange dstRange = srcRange;

	utils::imageMemoryBarrier(cmd, image,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_TRANSFER_READ_BIT,

		VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,

		srcRange);


	int32_t w_mip = w;
	int32_t h_mip = h;
	for (uint32_t i = 0; i < numOfMips - 1; ++i) {
		VkImageBlit region = {
			.srcSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.srcOffsets = {
				{ 0, 0, 0 },
				{ w_mip, h_mip, 1 }
			},
			.dstSubresource = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.mipLevel = i + 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.dstOffsets = {
				{ 0, 0, 0 },
				{ w_mip >> 1, h_mip >> 1, 1 }
			}
		};

		srcRange.baseMipLevel = i;
		dstRange.baseMipLevel = i + 1;

		if (i > 0) {
			utils::imageMemoryBarrier(cmd, image,
				VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_ACCESS_TRANSFER_READ_BIT,

				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,

				VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_PIPELINE_STAGE_TRANSFER_BIT,

				srcRange);
		}

		utils::imageMemoryBarrier(cmd, image,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_ACCESS_TRANSFER_WRITE_BIT,

			VK_IMAGE_LAYOUT_GENERAL,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,

			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,

			dstRange);

		vkCmdBlitImage(cmd,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &region, VK_FILTER_LINEAR
		);

		utils::imageMemoryBarrier(cmd, image,
			VK_ACCESS_TRANSFER_READ_BIT,
			VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,

			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_GENERAL,

			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

			srcRange);

		w_mip >>= 1;
		h_mip >>= 1;
	}

	utils::imageMemoryBarrier(cmd, image,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,

		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,

		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

		dstRange);
}

void Engine::recordCommandBuffer(FrameData& f, uint32_t imageIndex)
{
	loadDataToGPU();

	VkImageSubresourceRange range = {
			.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS
	};

	VkImageSubresourceRange depth_range{ range };
	depth_range.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

#if 1
	{ // Shadow pass
		cmdSetViewportScissor(f.cmd, _shadow.width, _shadow.height);

		bool anyMoved = false;
		for (int i = 0; i < MAX_LIGHTS; ++i) {
			if (!_renderContext.sceneData.lights[i].enabled ||
				!_renderContext.lightObjects[i]->HasMoved()) {
				continue;
			}
			// Update light view matrices based on lights current position
			glm::vec3 p = _renderContext.sceneData.lights[i].position;
			_renderContext.lightView = {
				glm::lookAt(p, p + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)),
				glm::lookAt(p, p + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)),
				glm::lookAt(p, p + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)),
				glm::lookAt(p, p + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)),
				glm::lookAt(p, p + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)),
				glm::lookAt(p, p + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0))
			};

			Material& mat = _materials["shadow"];
			vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat.pipeline);

			vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat.pipelineLayout, 0, 1, &f.shadowPassSet, 0, nullptr);

			for (uint32_t face = 0; face < 6; ++face) {
				updateCubeFace(f, i, face);
			}
			anyMoved = true;
		}

		if (anyMoved) {
#if ENABLE_SYNC == 1
			// Wait for shadow cubemap array to render before rendering the scene
			utils::imageMemoryBarrier(f.cmd, _shadow.cubemapArray.allocImage.image,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				VK_ACCESS_SHADER_READ_BIT,

				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

				range);
#elif ENABLE_SYNC == 2
			s_fullBarrier(f.cmd);
#endif
		}
	}
#endif

	cmdSetViewportScissor(f.cmd, _viewport.imageExtent.width, _viewport.imageExtent.height);
	{ // Viewport pass
		// Translate to required layout
		utils::imageMemoryBarrier(f.cmd, _viewport.images[imageIndex].image,
			0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

			range);

		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderingAttachmentInfoKHR color_attachment_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView = _viewport.imageViews[imageIndex],
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clearValues[0]
		};

		VkRenderingAttachmentInfoKHR depth_attachment_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView = _viewport.depth.view,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clearValues[1]
		};

		VkRect2D area = {
			.extent = {_viewport.imageExtent.width, _viewport.imageExtent.height}
		};

		VkRenderingInfoKHR renderingInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
			.renderArea = area,
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_info,
			.pDepthAttachment = &depth_attachment_info
		};

		// Viewport pass
		vkCmdBeginRendering(f.cmd, &renderingInfo);
		{
			drawObjects(f.cmd, _renderables);
		}
		vkCmdEndRendering(f.cmd);
	}

	// If the adaptation is disabled we are synchronizing straight to compute tone mapping step which also needs write acess
	int viewportToComputeDstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	if (!_renderContext.comp.enableAdaptation) {
		viewportToComputeDstAccessMask |= VK_ACCESS_SHADER_WRITE_BIT;
	}

	// Need to wait until scene is rendered to proceed with post-processing
	utils::imageMemoryBarrier(f.cmd, _viewport.images[imageIndex].image,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		viewportToComputeDstAccessMask, // Need only access to reading the image

		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,

		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

		range);

	{ // Compute step
#if 1
		if (_renderContext.comp.enableAdaptation) {
			// Compute luminance histogram
			ASSERT(MAX_LUMINANCE_BINS == 256);
			_compute.histogram.Bind(f.cmd)
				.UpdateImage(_viewport.imageViews[imageIndex], 2)
				.Dispatch(_viewport.imageExtent.width / 16 + 1, _viewport.imageExtent.height / 16 + 1, imageIndex)
				.Barrier();
#endif

#if 1
			// Compute average luminance
			_compute.averageLuminance.Bind(f.cmd)
				.Dispatch(1, 1, imageIndex)
				.Barrier();
#endif
		}
		
#if 1
		if (_renderContext.comp.enableLTM) {
			int threadsXY = 32;

			int groupsX = _viewport.imageExtent.width  / threadsXY + 1;
			int groupsY = _viewport.imageExtent.height / threadsXY + 1;
#if 0
			_compute.durand.stages[0].Bind(f.cmd)
				// In
				.UpdateImage(_viewport.imageViews[imageIndex], 2)
				// Out
				.UpdateImage(_compute.durand.att[0].view, 3)
				.UpdateImage(_compute.durand.att[1].view, 4)

				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			_compute.durand.stages[1].Bind(f.cmd)
				// In
				.UpdateImage(_compute.durand.att[0].view, 2)
				// Out
				.UpdateImage(_compute.durand.att[2].view, 3)
				.UpdateImage(_compute.durand.att[3].view, 4)

				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			_compute.durand.stages[2].Bind(f.cmd)
				// In
				.UpdateImage(_compute.durand.att[0].view, 2)
				.UpdateImage(_compute.durand.att[1].view, 3)
				.UpdateImage(_compute.durand.att[2].view, 4)
				.UpdateImage(_compute.durand.att[3].view, 5)
				// Out
				.UpdateImage(_viewport.imageViews[imageIndex], 6)

				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();
#else
			_compute.fusion.stages[0].Bind(f.cmd)
				// In
				.UpdateImage(_viewport.imageViews[imageIndex], 2)
				// Out
				.UpdateImage(_compute.fusion.chrominance, 3)
				.UpdateImage(_compute.fusion.luminance.views[0], 4)
				.UpdateImage(_compute.fusion.weight.views[0], 5)
				.UpdateImage(_compute.fusion.laplacian.views[0], 6)

				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();


			generateMips(f.cmd, _compute.fusion.luminance.allocImage.image, 
				_renderContext.comp.numOfViewportMips, 
				_viewport.imageExtent.width, _viewport.imageExtent.height);

			generateMips(f.cmd, _compute.fusion.weight.allocImage.image,
				_renderContext.comp.numOfViewportMips,
				_viewport.imageExtent.width, _viewport.imageExtent.height);

			_compute.fusion.stages[1].Bind(f.cmd)
				// In
				.UpdateImagePyramid(_compute.fusion.luminance, 2)
				// Out
				.UpdateImagePyramid(_compute.fusion.laplacian, 3)

				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			_compute.fusion.stages[2].Bind(f.cmd)
				// In
				.UpdateImagePyramid(_compute.fusion.laplacian, 2)
				.UpdateImagePyramid(_compute.fusion.weight, 3)
				// Out
				.UpdateImage(_compute.fusion.laplacianSum, 4)

				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			_compute.fusion.stages[3].Bind(f.cmd)
				// In
				.UpdateImage(_compute.fusion.chrominance, 2)
				.UpdateImage(_compute.fusion.laplacianSum, 3)
				// Out
				.UpdateImage(_viewport.imageViews[imageIndex], 4)

				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();
#endif
		}
#endif

#if 1	
		// Compute tone mapping
		_compute.toneMapping.Bind(f.cmd)
			.UpdateImage(_viewport.imageViews[imageIndex], 2)
			.Dispatch(_viewport.imageExtent.width / 32 + 1, _viewport.imageExtent.height / 32 + 1, imageIndex);
#endif
	}


#if ENABLE_SYNC == 1
	// Post-processing must finish because viewport image is sampled from during swapchain pass
	utils::imageMemoryBarrier(f.cmd, _viewport.images[imageIndex].image,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,

		VK_IMAGE_LAYOUT_GENERAL,
		//VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

		range);
#elif ENABLE_SYNC == 2
	s_fullBarrier(f.cmd);
#endif

	{
		// Traslate to required layout
		utils::imageMemoryBarrier(f.cmd, _swapchain.images[imageIndex],
			0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

			//VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Why doesn't this work?
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

			range);


		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderingAttachmentInfoKHR color_attachment_info = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView = _swapchain.imageViews[imageIndex],
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = clearValues[0]
		};

		VkRenderingInfo renderingInfo = {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
			.renderArea = {
				.extent = { _swapchain.imageExtent.width, _swapchain.imageExtent.height }
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_attachment_info,
			.pDepthAttachment = nullptr // Swapchain pass doesn't need depth attachment since it's just ImGui rendering to plane
		};
		
		// Swapchain pass
		vkCmdBeginRendering(f.cmd, &renderingInfo);
		{
			// Record dear imgui primitives into command buffer
			imguiOnRenderPassEnd(f.cmd);
		}
		vkCmdEndRendering(f.cmd);

		// Translate to required layout for presentation on screen
		utils::imageMemoryBarrier(f.cmd, _swapchain.images[imageIndex],
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,

			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,

			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,

			range);
	}
}

void Engine::drawFrame()
{
	FrameData& f = _frames[_frameInFlightNum];

	// Since we have descriptor set copies for each frame in flight,
	// we set the pointers to current frame's descriptor set buffers
	_gpu.Reset(f);

	// Needs to be part of drawFrame because drawFrame is called from onFramebufferResize callback
	imguiUpdate();
	imguiOnDrawStart();

    //Get index of next image after 
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_device, _swapchain.handle, UINT64_MAX, f.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            return;
        }
    }

	VKASSERT(vkWaitForFences(_device, 1, &f.inFlightFence, VK_TRUE, UINT64_MAX));
	VKASSERT(vkResetFences(_device, 1, &f.inFlightFence));

	{ // Command buffer
		VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info();

		VKASSERT(vkBeginCommandBuffer(f.cmd, &beginInfo));
		{
			recordCommandBuffer(f, imageIndex);
		}
		VKASSERT(vkEndCommandBuffer(f.cmd));
	}

    VkSemaphore waitSemaphores[] = { f.imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { f.renderFinishedSemaphore };

	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = waitSemaphores,
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &f.cmd,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = signalSemaphores
	};

    VKASSERT(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, f.inFlightFence));

    VkSwapchainKHR swapchains[] = { _swapchain.handle };
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &imageIndex
    };

    result = vkQueuePresentKHR(_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		return;
    }
    else {
        VKASSERTMSG(result, "failed to present swap chain image!");
    }

	_frameInFlightNum = (_frameInFlightNum + 1) % MAX_FRAMES_IN_FLIGHT;
}

