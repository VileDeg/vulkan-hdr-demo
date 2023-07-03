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
		std::vector<VkDescriptorSet> sets = { getCurrentFrame().globalSet, getCurrentFrame().objectSet };

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
	}

	// Load UNIFORM BUFFER of scene parameters to GPU
	{
		char* sd = (char*)_sceneParameterBuffer.gpu_ptr;
		sd += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;

		_renderContext.sceneData.cameraPos = _inp.camera.GetPos();


		memcpy(sd, &_renderContext.sceneData, sizeof(GPUSceneData));
	}
	
	{ // Load UNIFORM BUFFER of camera to GPU
		glm::mat4 viewMat = _inp.camera.GetViewMat();
		glm::mat4 projMat = _inp.camera.GetProjMat(_fovY, _viewport.imageExtent.width, _viewport.imageExtent.height);

		*_gpu.camera = {
			.view = viewMat,
			.proj = projMat,
			.viewproj = projMat * viewMat,
		};
	}

	{ // Load compute luminance SSBO to GPU
		_gpu.compLum->minLogLum = -2.f;
		_gpu.compLum->logLumRange = 12.f;
		_gpu.compLum->oneOverLogLumRange = 1.f / _gpu.compLum->logLumRange;
		_gpu.compLum->timeCoeff = 1.f;
		_gpu.compLum->totalPixelNum = _viewport.imageExtent.width * _viewport.imageExtent.height;

		for (size_t i = 0; i < MAX_LUMINANCE_BINS; ++i) {
			_gpu.compLum->luminance[i] = 0;
		}
	}

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

void Engine::recordCommandBuffer(FrameData& f, uint32_t imageIndex)
{
	VkClearValue colorClear{ .color = { 0.1f, 0.0f, 0.1f, 1.0f } };
	VkClearValue depthClear{ .depthStencil = { 1.0f, 0 } };
	VkClearValue clearValues[] = { colorClear, depthClear };

	VkRenderPassBeginInfo renderPassBeginInfo{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = _viewport.renderpass,
		.framebuffer = _viewport.framebuffers[imageIndex],
		.renderArea = {
			.offset = { 0, 0 },
			.extent = _viewport.imageExtent
		},
		.clearValueCount = 2,
		.pClearValues = clearValues
	};

	// Viewport pass
	vkCmdBeginRenderPass(f.cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		drawObjects(f.cmd, _renderables);
	}
	vkCmdEndRenderPass(f.cmd);

	{ // Sync graphics to compute
		VkImageMemoryBarrier imageMemoryBarrier = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_GENERAL,
			.newLayout = VK_IMAGE_LAYOUT_GENERAL,
			/* .image and .subresourceRange should identify image subresource accessed */ 
			.image = _viewportImages[imageIndex].image,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};

		vkCmdPipelineBarrier(
			f.cmd,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,          // dstStageMask
			0,
			0, nullptr,
			0, nullptr,
			1,                                             // imageMemoryBarrierCount
			&imageMemoryBarrier
		);
	}

	{ // Compute step
		// Compute luminance histogram
		vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.histogram.pipeline);
		{
			VkDescriptorImageInfo imageBufferInfo{
				.sampler = _linearSampler,
				.imageView = _viewport.imageViews[imageIndex],
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL
			};
			VkWriteDescriptorSet readonlyHDRImage =
				vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					f.compHistogramSet, &imageBufferInfo, 1);


			vkUpdateDescriptorSets(_device, 1, &readonlyHDRImage, 0, nullptr);
			vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.histogram.pipelineLayout, 0, 1, &f.compHistogramSet, 0, nullptr);

			VkMemoryBarrier memoryBarrier = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
			};

			vkCmdPipelineBarrier(
				f.cmd,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // srcStageMask
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // dstStageMask
				0,
				1, &memoryBarrier,
				0, nullptr,
				0, nullptr
			);

	
			ASSERT(MAX_LUMINANCE_BINS == 256);
			vkCmdDispatch(f.cmd, 16, 16, 1);
		}

	

		// Compute average luminance
		vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.averageLuminance.pipeline);
		{
			vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.averageLuminance.pipelineLayout, 0, 1, &f.compAvgLumSet, 0, nullptr);

			VkMemoryBarrier memoryBarrier = {
				.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT
			};

			vkCmdPipelineBarrier(
				f.cmd,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // srcStageMask
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // dstStageMask
				0,
				1, &memoryBarrier,
				0, nullptr,
				0, nullptr
			);

			vkCmdDispatch(f.cmd, MAX_LUMINANCE_BINS, 1, 1);
		}
	}


	renderPassBeginInfo.renderPass = _swapchain.renderpass;
	renderPassBeginInfo.framebuffer = _swapchain.framebuffers[imageIndex];
	renderPassBeginInfo.renderArea.extent = _swapchain.imageExtent;

	// Swapchain pass
	vkCmdBeginRenderPass(f.cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		// Record dear imgui primitives into command buffer
		imguiOnRenderPassEnd(f.cmd);
	}
	vkCmdEndRenderPass(f.cmd);
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
    VkResult result = vkAcquireNextImageKHR(_device, _swapchainHandle, UINT64_MAX, f.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
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

    VkSwapchainKHR swapchains[] = { _swapchainHandle };
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

