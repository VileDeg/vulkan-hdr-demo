#include "stdafx.h"
#include "engine.h"

#include "imgui/imgui.h"

static int getLumIndex(int(&lumHist)[MAX_LUMINANCE_BINS], int matchPx) {
	int i = 0;
	int lum_sum = 0;
	while (i < MAX_LUMINANCE_BINS) {
		if ((lum_sum + lumHist[i]) > matchPx) {
			/*if ((lum_sum + lumHist[i] / 2) < matchPx) {
				++i;
			}*/
			break;
		}
		lum_sum += lumHist[i];
		++i;
	}

	i = std::clamp(i, 0, MAX_LUMINANCE_BINS-1);
	return i;
}

void Engine::drawObject(VkCommandBuffer cmd, const std::shared_ptr<RenderObject>& object, Material** lastMaterial, Mesh** lastMesh, int index) {
	const RenderObject& obj = *object;
	Model* model = obj.model;

	for (int m = 0; m < model->meshes.size(); ++m) {
		Mesh* mesh = model->meshes[m];

		ASSERT(mesh && mesh->material);

		//offset for our scene buffer
		uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;

		// Always add the sceneData and SSBO descriptor
		std::vector<VkDescriptorSet> sets = { getCurrentFrame().globalDescriptor, getCurrentFrame().objectDescriptor };

		{ // Push diffuse texture descriptor set
			VkImageView imageView = VK_NULL_HANDLE;
			if (mesh->p_tex != nullptr && !obj.isSkybox) {
				imageView = mesh->p_tex->imageView;
			}
			VkDescriptorImageInfo imageBufferInfo{
				.sampler = _linearSampler,
				.imageView = imageView,
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};
			VkWriteDescriptorSet texture1 =
				vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					VK_NULL_HANDLE, &imageBufferInfo, 0);
			vkCmdPushDescriptorSetKHR(
				cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipelineLayout, 2, 1, &texture1);
		}

		// Load push constants to GPU
		GPUPushConstantData pc = {
			.hasTexture = (mesh->p_tex != nullptr),
			.lightAffected = model->lightAffected,
			.isCubemap = obj.isSkybox
		};
		/*_renderContext.pushConstantData = {
			.hasTexture = (mesh->p_tex != nullptr),
			.lightAffected = model->lightAffected,
			.isCubemap = obj.isSkybox
		};*/

		vkCmdPushConstants(
			cmd, mesh->material->pipelineLayout,
			VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUPushConstantData),
			&pc);

		// If the material is different, bind the new material
		if (mesh->material != *lastMaterial) {
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipeline);

			{
				VkViewport viewport{
					.x = 0.0f,
					.y = 0.0f,
					.width = static_cast<float>(_viewport.imageExtent.width),
					.height = static_cast<float>(_viewport.imageExtent.height),
					.minDepth = 0.0f,
					.maxDepth = 1.0f
				};

				vkCmdSetViewport(cmd, 0, 1, &viewport);

				VkRect2D scissor{
					.offset = { 0, 0 },
					.extent = { _viewport.imageExtent.width, _viewport.imageExtent.height }
				};

				vkCmdSetScissor(cmd, 0, 1, &scissor);
			}


			*lastMaterial = mesh->material;
			// Bind the descriptor sets
			vkCmdBindDescriptorSets(
				cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipelineLayout, 0, sets.size(), sets.data(), 1, &uniform_offset);
		}

		if (mesh != *lastMesh) {
			VkDeviceSize zeroOffset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer.buffer, &zeroOffset);

			vkCmdBindIndexBuffer(cmd, mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			*lastMesh = mesh;
		}

		// Ve send loop index as instance index to use it in shader to access object data in SSBO
		vkCmdDrawIndexed(cmd, mesh->indices.size(), 1, 0, 0, index);
	}
}

void Engine::drawObjects(VkCommandBuffer cmd, const std::vector<std::shared_ptr<RenderObject>>& objects)
{
	


	// Load SSBO to GPU
	{
		_renderContext.ssboConfigs.exposureON	 = _inp.exposureEnabled;
		_renderContext.ssboConfigs.toneMappingON = _inp.toneMappingEnabled;

		memcpy(&_gpu.ssbo->configs, &_renderContext.ssboConfigs, sizeof(SSBOConfigs));

		for (int i = 0; i < objects.size(); i++) {
			_gpu.ssbo->objects[i].modelMatrix = objects[i]->Transform();
			_gpu.ssbo->objects[i].color = objects[i]->color;
			_gpu.ssbo->objects[i].useObjectColor = objects[i]->model->useObjectColor;
		}
		//unsigned int oldMax = _gpu.ssbo->oldMax;
		std::swap(_gpu.ssbo->newMax, _gpu.ssbo->oldMax);

		// For optimization purposes we assume that MAX of new frame
		// won't be more then two times lower.
		//float nm = 0.5f * oldMax;
		float nm = 0.f;

		_gpu.ssbo->newMax = *reinterpret_cast<unsigned int*>(&nm);;
#if 0
		constexpr size_t arr_size = ARRAY_SIZE(_gpu.ssbo->luminance);

		float f_oldMax = *reinterpret_cast<float*>(&_gpu.ssbo->oldMax);

		/*int start_i = (arr_size-1) * _renderContext.luminanceHistogramBounds.x;
		int end_i	= (arr_size-1) * _renderContext.luminanceHistogramBounds.y;*/

		int total_px = _viewport.imageExtent.width * _viewport.imageExtent.height;

		int total_px_hist = 0;
		for (auto& px : _gpu.ssbo->luminance) {
			total_px_hist += px;
		}

		if (total_px_hist > 0) { // If histogram was populated
			//ASSERT(total_px == total_px_hist);

			/*int start_px = total_px_hist * _renderContext.luminanceHistogramBounds.x;
			int end_px = total_px_hist * _renderContext.luminanceHistogramBounds.y;

			int& start_i = _renderContext.lumHistStartI;
			int& end_i = _renderContext.lumHistEndI;

			start_i = getLumIndex(_gpu.ssbo->luminance, start_px);
			end_i = getLumIndex(_gpu.ssbo->luminance, end_px);

			ASSERT(start_i <= end_i);*/

			int start_i = 0;
			int end_i = MAX_LUMINANCE_BINS - 1;

			unsigned int sumPx = 0;
			for (int i = start_i; i < end_i; ++i) {
				/*float lum = (float)i / MAX_LUMINANCE_BINS * f_oldMax;
				float loglum = std::log2(1 + lum);*/
				//sum += loglum;
				//lums.push_back(lum);
				
				// Number of pixels in bin weighted by index
				unsigned int weightedPx = _gpu.ssbo->luminance[i] * i;
				sumPx += weightedPx;
			}

			float weightedLogAverage = (sumPx / std::max((float)total_px - _gpu.ssbo->luminance[0], 1.f)) - 1.f;

			float logLumRange = 1.f / _renderContext.sceneData.oneOverLogLumRange;
			float weightedAverageLuminance = exp2(((weightedLogAverage / (float)(MAX_LUMINANCE_BINS - 1)) * 
				logLumRange) + _renderContext.sceneData.minLogLum);

			// Compute common
			//float avgLum = 0.f;
			//if (end_i - start_i > 0) {
			//	avgLum = sum / (end_i - start_i);
			//}
			////float avg = 1;
			////float exposureAvg = 9.6 * (avg + 0.0001);
			//float targetAdp = avgLum;

			_renderContext.targetExposure = weightedAverageLuminance;


			float a = _renderContext.exposureBlendingFactor * _deltaTime;
			//a = std::clamp(a, 0.0001f, 0.99f);
			// Final exposure
			float prevAdp = _gpu.ssbo->exposureAverage;
			float adaptedLum = prevAdp + (weightedAverageLuminance - prevAdp) * a;
			_gpu.ssbo->exposureAverage = adaptedLum;
		}

		// Clear luminance from previous frame
		memset(_gpu.ssbo->luminance, 0, sizeof(_gpu.ssbo->luminance));
#endif

		_gpu.ssbo->exposureAverage = 1.f;
	}

	// Load UNIFORM BUFFER of scene parameters to GPU
	{
		//pr("Cam pos: " << V3PR(_renderContext.gpu_sd->cameraPos));

		// Do not use _renderContext.gpu_sd instead of _sceneParameterBuffer.gpu_ptr!!
		//char* padded = (char*)(_sceneParameterBuffer.gpu_ptr) + pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;

		char* sd = (char*)_sceneParameterBuffer.gpu_ptr;
		sd += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;
		//GPUSceneData* sd = (GPUSceneData*)(padded);

		_renderContext.sceneData.cameraPos = _inp.camera.GetPos();


		memcpy(sd, &_renderContext.sceneData, sizeof(GPUSceneData));

		//_renderContext.gpu_sd->cameraPos = _inp.camera.GetPos();

		

		/*char* sceneData = (char*)sc;
		sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;
		memcpy(sceneData, &_renderContext.sceneData, sizeof(GPUSceneData));*/

		//GPUSceneData sd = {}

		/*char* sceneData = (char*)_sceneParameterBuffer.gpu_ptr;
		sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;
		memcpy(sceneData, &_renderContext.sceneData, sizeof(GPUSceneData));*/
		
		//_sceneParameterBuffer.runOnMemoryMap(_allocator, // TODO: don't unmap memory
		//	[&](void* data) {
		//		char* sceneData = (char*)data;
		//		sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;
		//		memcpy(sceneData, &_renderContext.sceneData, sizeof(GPUSceneData));
		//	}
		//);
	}

	

	glm::mat4 viewMat = _inp.camera.GetViewMat();
	glm::mat4 projMat = _inp.camera.GetProjMat(_fovY, _viewport.imageExtent.width, _viewport.imageExtent.height);

	/*GPUCameraData camData{
		.view = viewMat,
		.proj = projMat,
		.viewproj = projMat * viewMat,
	};*/

	*_gpu.camera = {
		.view = viewMat,
		.proj = projMat,
		.viewproj = projMat * viewMat,
	};

	//getCurrentFrame().cameraBuffer.runOnMemoryMap(_allocator, // TODO: don't unmap memory
	//	[&](void* data) {
	//		memcpy(data, &camData, sizeof(GPUCameraData));
	//	}
	//);

	Mesh* lastMesh = nullptr;
	Material* lastMaterial = nullptr;
	for (int i = 0; i < objects.size(); i++) {
		drawObject(cmd, objects[i], &lastMaterial, &lastMesh, i);
	}

	if (_renderContext.enableSkybox) {
		// Draw skybox as the last object
		drawObject(cmd, _skyboxObject, &lastMaterial, &lastMesh, 0);
	}
}

void Engine::drawFrame()
{
	_frameInFlightNum = (_frameNumber) % MAX_FRAMES_IN_FLIGHT;
	FrameData& f = _frames[_frameInFlightNum];

	

	// Set general GPU pointers to current frame's ones. It's handy.
	// And they can be accessed in other files (for example UI)
	_gpu.Reset(getCurrentFrame());

	// Needs to be part of drawFrame because drawFrame is called from onFramebufferResize callback
	imguiUpdate();

    

    VKASSERT(vkWaitForFences(_device, 1, &f.inFlightFence, VK_TRUE, UINT64_MAX));
    VKASSERT(vkResetFences(_device, 1, &f.inFlightFence));

    //Get index of next image after 
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_device, _swapchain.handle, UINT64_MAX, f.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        } else {
            ASSERTMSG(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "failed to acquire swap chain image!");
        }
    }

	// Calls ImGui::Render()
	imguiOnDrawStart();

	{ // Command buffer
		VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info();

		VKASSERT(vkBeginCommandBuffer(f.cmdBuffer, &beginInfo));
		{
			VkClearValue colorClear{ .color = { 0.1f, 0.0f, 0.1f, 1.0f } };
			VkClearValue depthClear{ .depthStencil = { 1.0f, 0 } };
			VkClearValue clearValues[] = { colorClear, depthClear };

			VkRenderPassBeginInfo renderPassBeginInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = _viewportRenderpass,
				.framebuffer = _viewport.framebuffers[imageIndex],
				.renderArea = {
					.offset = { 0, 0 },
					.extent = _viewport.imageExtent
				},
				.clearValueCount = 2,
				.pClearValues = clearValues
			};

			// Viewport pass
			vkCmdBeginRenderPass(f.cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				drawObjects(f.cmdBuffer, _renderables);
			}
			vkCmdEndRenderPass(f.cmdBuffer);

			renderPassBeginInfo.renderPass = _mainRenderpass;
			renderPassBeginInfo.framebuffer = _swapchain.framebuffers[imageIndex];
			renderPassBeginInfo.renderArea.extent = _swapchain.imageExtent;

			// Swapchain pass
			vkCmdBeginRenderPass(f.cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				// Record dear imgui primitives into command buffer
				imguiOnRenderPassEnd(f.cmdBuffer);
			}
			vkCmdEndRenderPass(f.cmdBuffer);
		}
		VKASSERT(vkEndCommandBuffer(f.cmdBuffer));
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
		.pCommandBuffers = &f.cmdBuffer,
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

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _inp.framebufferResized) {
        //_framebufferResized = false;
        recreateSwapchain();
    }
    else {
        VKASSERTMSG(result, "failed to present swap chain image!");
    }

    ++_frameNumber;
}

