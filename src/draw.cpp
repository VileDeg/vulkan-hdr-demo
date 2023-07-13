#include "stdafx.h"
#include "engine.h"

#include "imgui/imgui.h"

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
		std::vector<VkDescriptorSet> sets = { getCurrentFrame().globalSet }; //, getCurrentFrame().objectSet 

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
	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = 
		vkinit::renderpass_begin_info(_shadow.renderpass, { _shadow.width, _shadow.height }, _shadow.faceFramebuffers[lightIndex][faceIndex]);
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	// Render scene from cube face's point of view
	vkCmdBeginRenderPass(f.cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	

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

	/*Material& mat = _materials["shadow"];
	vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat.pipeline);

	uint32_t uniform_offset = pad_uniform_buffer_size(sizeof(GPUSceneUB)) * _frameInFlightNum;
	vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat.pipelineLayout, 0, 1, &f.shadowPassSet, 1, &uniform_offset);*/

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

	vkCmdEndRenderPass(f.cmd);
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

		uint sum_skipped = 0;

		for (uint i = 0; i < MAX_LUMINANCE_BINS; ++i) {
			//tmp_sum += _gpu.compSSBO->luminance[i];

			sum += _gpu.compSSBO->luminance[i];
			if (sum > lb) {
				li = i;
				break;
			}
		}

		sum_skipped += sum;

		for (uint i = li + 1; i < MAX_LUMINANCE_BINS; ++i) {
			//tmp_sum += _gpu.compSSBO->luminance[i];
			sum += _gpu.compSSBO->luminance[i];
			if (sum > ub) {
				ui = i;
				break;
			}
		}

		sum_skipped += _renderContext.comp.totalPixelNum - sum;

		_renderContext.comp.logLumRange = _renderContext.maxLogLuminance - _renderContext.comp.minLogLum;
		_renderContext.comp.oneOverLogLumRange = 1.f / _renderContext.comp.logLumRange;
		_renderContext.comp.totalPixelNum = _viewport.imageExtent.width * _viewport.imageExtent.height;
		_renderContext.comp.timeCoeff = 1 - std::exp(-_deltaTime * _renderContext.eyeAdaptationTimeCoefficient);
		_renderContext.comp.lumLowerIndex = li;
		_renderContext.comp.lumUpperIndex = ui;

		memcpy(_gpu.compUB, &_renderContext.comp, sizeof(GPUCompUB));

		// Reset luminance from previous frame
		memset(_gpu.compSSBO->luminance, 0, sizeof(_gpu.compSSBO->luminance));
	}
}

void Engine::recordCommandBuffer(FrameData& f, uint32_t imageIndex)
{
	loadDataToGPU();

	{ // Shadow pass
		cmdSetViewportScissor(f.cmd, _shadow.width, _shadow.height);

		for (int i = 0; i < MAX_LIGHTS; ++i) {
			if (!_renderContext.sceneData.lights[i].enabled) {
				continue;
			}
			if (!_renderContext.lightObjects[i]->HasMoved()) {
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
		}
	}

	VkClearValue clearValues[] = { {.color = { 0.1f, 0.0f, 0.1f, 1.0f } }, {.depthStencil = { 1.0f, 0 } } };

	VkRenderPassBeginInfo renderPassBeginInfo =
		vkinit::renderpass_begin_info(_viewport.renderpass, _viewport.imageExtent, _viewport.framebuffers[imageIndex]);
	renderPassBeginInfo.pClearValues = clearValues;
	renderPassBeginInfo.clearValueCount = 2;


	cmdSetViewportScissor(f.cmd, _viewport.imageExtent.width, _viewport.imageExtent.height);

	// Viewport pass
	vkCmdBeginRenderPass(f.cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		drawObjects(f.cmd, _renderables);
	}
	vkCmdEndRenderPass(f.cmd);

	{ // Compute step
		if (_renderContext.comp.enableAdaptation) {
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
						f.compHistogramSet, &imageBufferInfo, 2);


				vkUpdateDescriptorSets(_device, 1, &readonlyHDRImage, 0, nullptr);
				vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.histogram.pipelineLayout, 0, 1, &f.compHistogramSet, 0, nullptr);

				ASSERT(MAX_LUMINANCE_BINS == 256);
				constexpr uint32_t thread_size = 16;
				vkCmdDispatch(f.cmd, _viewport.imageExtent.width / thread_size + 1, _viewport.imageExtent.height / thread_size + 1, 1);
			}

			// Compute average luminance
			vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.averageLuminance.pipeline);
			{
				vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.averageLuminance.pipelineLayout, 0, 1, &f.compAvgLumSet, 0, nullptr);

				// Need to run just one group of MAX_LUMINANCE_BINS to calculate average of luminance array
				vkCmdDispatch(f.cmd, 1, 1, 1);
			}
		}

		// Compute tone mapping
		vkCmdBindPipeline(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.toneMapping.pipeline);
		{
			VkDescriptorImageInfo imageBufferInfo{
				.sampler = _linearSampler,
				.imageView = _viewport.imageViews[imageIndex],
				.imageLayout = VK_IMAGE_LAYOUT_GENERAL
			};
			VkWriteDescriptorSet inOutHDRImage =
				vkinit::write_descriptor_image(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
					f.compTonemapSet, &imageBufferInfo, 2);

			vkUpdateDescriptorSets(_device, 1, &inOutHDRImage, 0, nullptr);

			vkCmdBindDescriptorSets(f.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _compute.toneMapping.pipelineLayout, 0, 1, &f.compTonemapSet, 0, nullptr);

			constexpr uint32_t thread_size = 32;
			vkCmdDispatch(f.cmd, _viewport.imageExtent.width / thread_size + 1, _viewport.imageExtent.height / thread_size + 1, 1);
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

	// Since we have descriptor set copies for each frame in flight,
	// we set the pointers to current frame's descriptor set buffers
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

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _framebufferResized) {
        //_framebufferResized = false;
        recreateSwapchain();
    }
    else {
        VKASSERTMSG(result, "failed to present swap chain image!");
    }

    ++_frameNumber;
}

