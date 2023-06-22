#include "stdafx.h"
#include "engine.h"

#include "imgui/imgui.h"

void Engine::drawObjects(VkCommandBuffer cmd, const std::vector<std::shared_ptr<RenderObject>>& objects)
{
	// Load SSBO to GPU
	{
		auto& sd = _renderContext.ssboData;

		sd.exposureON = _inp.exposureEnabled;
		sd.toneMappingON = _inp.toneMappingEnabled;

		for (int i = 0; i < objects.size(); i++) {

			sd.objects[i].modelMatrix = objects[i]->Transform();
			sd.objects[i].color = objects[i]->color;
			sd.objects[i].useObjectColor = objects[i]->model->useObjectColor;
		}

		// Clear luminance from previous frame
		memset(sd.luminance, 0, sizeof(sd.luminance));

		getCurrentFrame().objectBuffer.runOnMemoryMap(_allocator,
			[&](void* data) {
				GPUSSBOData* gpuSD = (GPUSSBOData*)data;

				

				unsigned int oldMax = gpuSD->oldMax;
				std::swap(gpuSD->newMax, gpuSD->oldMax);

				// For optimization purposes we assume that MAX of new frame
				// won't be more then two times lower.
				gpuSD->newMax = 0.5f * oldMax;

				auto& sd = _renderContext.ssboData;
				sd.newMax = gpuSD->newMax;
				sd.oldMax = gpuSD->oldMax;

				constexpr size_t arr_size = ARRAY_SIZE(gpuSD->luminance);

				float f_oldMax = *reinterpret_cast<float*>(&gpuSD->oldMax);

				// Skip N%
				int start_i = arr_size * _renderContext.luminanceHistogramBounds.x;
				int end_i	= arr_size * _renderContext.luminanceHistogramBounds.y;
				// Find the bin with maximum pixels
				int max_i = start_i;
				int maxBin = 0;

				float sum = 0;

				for (int i = start_i; i < end_i; ++i) {
					int val = gpuSD->luminance[i].val;
					if (val > maxBin) {
						maxBin = val;
						max_i = i;
					}
					float lum = (float)i / MAX_LUMINANCE_BINS * f_oldMax;
					sum += lum;
				}

				float avg = sum / (end_i - start_i);

				// Compute common luminance
				//sd.commonLuminance = (float)max_i / MAX_LUMINANCE_BINS * f_oldMax;
				sd.commonLuminance = avg;

				// Store GPU luminance values before clearing them up
				Lum tmpLum[arr_size];
				memcpy(tmpLum, gpuSD->luminance, sizeof(tmpLum));
				
				// Load SSBO data to GPU
				memcpy(data, &sd, sizeof(GPUSSBOData));

				// Copy GPU luminance to CPU side
				memcpy(sd.luminance, tmpLum, sizeof(tmpLum));
			}
		);
	}

	// Load UNIFORM BUFFER of scene parameters to GPU
	{
		_renderContext.sceneData.cameraPos = _inp.camera.GetPos();
		_sceneParameterBuffer.runOnMemoryMap(_allocator,
			[&](void* data) {
				char* sceneData = (char*)data;
				sceneData += pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;
				memcpy(sceneData, &_renderContext.sceneData, sizeof(GPUSceneData));
			}
		);
	}

	uint32_t extentX = _viewport.imageExtent.width;
	uint32_t extentY = _viewport.imageExtent.height;

	glm::mat4 viewMat = _inp.camera.GetViewMat();
	glm::mat4 projMat = _inp.camera.GetProjMat(_fovY, extentX, extentY);

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
		Model* model = obj.model;

		for (int m = 0; m < model->meshes.size(); ++m) {
			Mesh* mesh = model->meshes[m];

			ASSERT(mesh && mesh->material);

			//offset for our scene buffer
			uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneData)) * _frameInFlightNum;

			// Always add the sceneData and SSBO descriptor
			std::vector<VkDescriptorSet> sets = { getCurrentFrame().globalDescriptor, getCurrentFrame().objectDescriptor };

			VkImageView imageView = VK_NULL_HANDLE;
			if (mesh->p_tex != nullptr) {
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

			_renderContext.pushConstantData.hasTexture = (mesh->p_tex != nullptr);
			_renderContext.pushConstantData.lightAffected = model->lightAffected;

			vkCmdPushConstants(
				cmd, mesh->material->pipelineLayout,
				VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GPUPushConstantData),
				&_renderContext.pushConstantData);

			// If the material is different, bind the new material
			if (mesh->material != lastMaterial) {
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipeline);

				{
					VkViewport viewport{
						.x = 0.0f,
						.y = 0.0f,
						.width = static_cast<float>(extentX),
						.height = static_cast<float>(extentY),
						.minDepth = 0.0f,
						.maxDepth = 1.0f
					};

					vkCmdSetViewport(cmd, 0, 1, &viewport);

					VkRect2D scissor{
						.offset = { 0, 0 },
						.extent = { extentX, extentY }
					};

					vkCmdSetScissor(cmd, 0, 1, &scissor);
				}


				lastMaterial = mesh->material;
				// Bind the descriptor sets
				vkCmdBindDescriptorSets(
					cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->material->pipelineLayout, 0, sets.size(), sets.data(), 1, &uniform_offset);
			}

			if (mesh != lastMesh) {
				VkDeviceSize zeroOffset = 0;
				vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer.buffer, &zeroOffset);

				vkCmdBindIndexBuffer(cmd, mesh->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

				lastMesh = mesh;
			}

			// Ve send loop index as instance index to use it in shader to access object data in SSBO
			vkCmdDrawIndexed(cmd, mesh->indices.size(), 1, 0, 0, i);
		}
	}
}

void Engine::drawFrame()
{
	// Needs to be part of drawFrame because drawFrame is called from onFramebufferResize callback
	imguiCommands();

    _frameInFlightNum = (_frameNumber) % MAX_FRAMES_IN_FLIGHT;
    FrameData& frame = _frames[_frameInFlightNum];

    VKASSERT(vkWaitForFences(_device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX));
    VKASSERT(vkResetFences(_device, 1, &frame.inFlightFence));

    //Get index of next image after 
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_device, _swapchain.handle, UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        } else {
            ASSERTMSG(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "failed to acquire swap chain image!");
        }
    }

	// Following calls ImGui::Render()
	imguiOnDrawStart();

	{ // Command buffer
		VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info();

		VKASSERT(vkBeginCommandBuffer(frame.cmdBuffer, &beginInfo));
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
			vkCmdBeginRenderPass(frame.cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				drawObjects(frame.cmdBuffer, _renderables);
			}
			vkCmdEndRenderPass(frame.cmdBuffer);

			renderPassBeginInfo.renderPass = _mainRenderpass;
			renderPassBeginInfo.framebuffer = _swapchain.framebuffers[imageIndex];
			renderPassBeginInfo.renderArea.extent = _swapchain.imageExtent;

			// Swapchain pass
			vkCmdBeginRenderPass(frame.cmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				// Record dear imgui primitives into command buffer
				imguiOnRenderPassEnd(frame.cmdBuffer);
			}
			vkCmdEndRenderPass(frame.cmdBuffer);
		}
		VKASSERT(vkEndCommandBuffer(frame.cmdBuffer));
	}

    VkSemaphore waitSemaphores[] = { frame.imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { frame.renderFinishedSemaphore };

	VkSubmitInfo submitInfo{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = waitSemaphores,
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &frame.cmdBuffer,
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = signalSemaphores
	};

    VKASSERT(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, frame.inFlightFence));

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

