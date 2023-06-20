#include "stdafx.h"
#include "Engine.h"

void Engine::drawObjects(VkCommandBuffer cmd, const std::vector<std::shared_ptr<RenderObject>>& objects)
{
	// Load SSBO to GPU
	{
		UpdateSSBOData(objects);
		getCurrentFrame().objectBuffer.runOnMemoryMap(_allocator,
			[&](void* data) {
				char* ssboData = (char*)data;
				unsigned int* newMax = (unsigned int*)(ssboData + offsetof(GPUSSBOData, newMax));
				unsigned int* oldMax = (unsigned int*)(ssboData + offsetof(GPUSSBOData, oldMax));
				float* f_oldMax = reinterpret_cast<float*>(oldMax);

				if (_frameNumber == 0) { // Initially new MAX and old MAX are zero.
					*newMax = 0;
					*oldMax = 0;
				} else { // On every next frame, swap new MAX and old MAX.
					unsigned int tmp = *oldMax;
					std::swap(*newMax, *oldMax);
					// For optimization we assume that MAX of new frame
					// won't be more then two times lower.
					*newMax = 0.5 * tmp;
				}
				auto& sd = _renderContext.ssboData;
				sd.newMax = *newMax;
				sd.oldMax = *oldMax;

				memcpy(data, &sd, sizeof(GPUSSBOData));
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
				bindPipeline(cmd, mesh->material->pipeline);
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
    imguiOnDrawStart();

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

	{ // Viewport command buffer
		VKASSERT(vkResetCommandBuffer(frame.viewportCmdBuffer, 0));

		VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info();

		VKASSERT(vkBeginCommandBuffer(frame.viewportCmdBuffer, &beginInfo));
		{
			

			VkClearValue colorClear{
				.color = { 0.1f, 0.0f, 0.1f, 1.0f }
			};

			VkClearValue depthClear{
				.depthStencil = { 1.0f, 0 }
			};

			VkClearValue clearValues[] = { colorClear, depthClear };

			VkRenderPassBeginInfo renderPassBeginInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = _viewportRenderpass,
				.framebuffer = _viewport.framebuffers[imageIndex],
				.renderArea = {
					.offset = { 0, 0 },
					.extent = _windowExtent
				},
				.clearValueCount = 2,
				.pClearValues = clearValues
			};



			vkCmdBeginRenderPass(frame.viewportCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				

				drawObjects(frame.viewportCmdBuffer, _renderables);
			}
			vkCmdEndRenderPass(frame.viewportCmdBuffer);
		}
		VKASSERT(vkEndCommandBuffer(frame.viewportCmdBuffer));
	}

	{ // Main command buffer
		VKASSERT(vkResetCommandBuffer(frame.mainCmdBuffer, 0));

		VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info();

		VKASSERT(vkBeginCommandBuffer(frame.mainCmdBuffer, &beginInfo));
		{
			// Create a VkImageMemoryBarrier struct
			VkImageMemoryBarrier barrier = {};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcAccessMask = 0; // Specify the previous access mask for the image
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // Specify the desired access mask for shader reads
			barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Specify the current layout of the image
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Specify the desired layout for shader reads
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // Specify the source queue family index
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // Specify the destination queue family index
			barrier.image = _viewport.images[imageIndex].image; // Specify the VkImage object
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; // Specify the image aspect mask
			barrier.subresourceRange.baseMipLevel = 0; // Specify the base mip level
			barrier.subresourceRange.levelCount = 1; // Specify the number of mip levels
			barrier.subresourceRange.baseArrayLayer = 0; // Specify the base array layer
			barrier.subresourceRange.layerCount = 1; // Specify the number of array layers

			// Transition the image layout
			vkCmdPipelineBarrier(
				frame.mainCmdBuffer,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0,
				0,
				nullptr,
				0,
				nullptr,
				1,
				&barrier
			);

			VkClearValue colorClear{
				.color = { 0.1f, 0.0f, 0.1f, 1.0f }
			};

			VkClearValue depthClear{
				.depthStencil = { 1.0f, 0 }
			};

			VkClearValue clearValues[] = { colorClear, depthClear };

			VkRenderPassBeginInfo renderPassBeginInfo{
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = _mainRenderpass,
				.framebuffer = _swapchain.framebuffers[imageIndex],
				.renderArea = {
					.offset = { 0, 0 },
					.extent = _windowExtent
				},
				.clearValueCount = 2,
				.pClearValues = clearValues
			};

			vkCmdBeginRenderPass(frame.mainCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			{
				// Record dear imgui primitives into command buffer
				imguiOnRenderPassEnd(frame.mainCmdBuffer);
			}
			vkCmdEndRenderPass(frame.mainCmdBuffer);
		}
		VKASSERT(vkEndCommandBuffer(frame.mainCmdBuffer));
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
        .pCommandBuffers = &frame.mainCmdBuffer,
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

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _framebufferResized) {
        _framebufferResized = false;
        recreateSwapchain();
    }
    else {
        VKASSERTMSG(result, "failed to present swap chain image!");
    }

    ++_frameNumber;
}

