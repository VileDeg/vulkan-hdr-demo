#include "stdafx.h"
#include "engine.h"

#include "imgui/imgui.h"

#define COMPUTE_THREADS_XY 32
#define FUSION_DBG_PREF std::string("LTM::FUSION")
#define DURAND_DBG_PREF std::string("LTM::DURAND")

constexpr VkImageSubresourceRange FULL_COLOR_RANGE = {
	.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	.baseMipLevel = 0,
	.levelCount = VK_REMAINING_MIP_LEVELS,
	.baseArrayLayer = 0,
	.layerCount = VK_REMAINING_ARRAY_LAYERS
};

constexpr VkClearValue CDS_CLEAR_VALUES[2] = {
	{.color = {.float32 = { 0.0f, 0.0f, 0.0f, 0.0f } } },
	{.depthStencil = {.depth = 1.0f, .stencil = 0 } }
};

#if 0
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
#endif

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

void Engine::updateShadowCubemapFace(FrameData& f, uint32_t lightIndex, uint32_t faceIndex)
{
	// Translate to required layout
	vk_utils::imageMemoryBarrier(f.cmd, _shadow.cubemapArray.allocImage.image,
		0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

		FULL_COLOR_RANGE);

	VkRenderingAttachmentInfo colorAttachmentInfo = vkinit::rendering_attachment_info(_shadow.faceViews[lightIndex][faceIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	VkRenderingAttachmentInfo depthAttachmentInfo = vkinit::rendering_attachment_info(_shadow.depth.view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderingInfo = vkinit::rendering_info(&colorAttachmentInfo, &depthAttachmentInfo, _shadow.width, _shadow.height);

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
	vk_utils::imageMemoryBarrier(f.cmd, _shadow.cubemapArray.allocImage.image,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,

		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

		FULL_COLOR_RANGE);
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
		_renderContext.sceneData.cameraPos = _camera.GetPos();
		_renderContext.sceneData.lightFarPlane = _renderContext.zFar;

		memcpy(_gpu.scene, &_renderContext.sceneData, sizeof(GPUSceneUB));
	}

	{ // Load UNIFORM BUFFER of camera to GPU
		glm::mat4 viewMat = _camera.GetViewMat();
		glm::mat4 projMat = _camera.GetProjMat(_fovY, _viewport.width, _viewport.height);

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
		float lb = _postfx.lumPixelLowerBound * _postfx.ub.totalPixelNum;
		float ub = _postfx.lumPixelUpperBound * _postfx.ub.totalPixelNum;

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

		_postfx.ub.logLumRange = _postfx.maxLogLuminance - _postfx.ub.minLogLum;
		_postfx.ub.oneOverLogLumRange = 1.f / _postfx.ub.logLumRange;
		_postfx.ub.totalPixelNum = _viewport.width * _viewport.height;
		_postfx.ub.timeCoeff = 1 - std::exp(-_deltaTime * _postfx.eyeAdaptationTimeCoefficient);
		_postfx.ub.lumLowerIndex = li;
		_postfx.ub.lumUpperIndex = ui;
		float avg = (_viewport.width + _viewport.height) / 2;
		_postfx.ub.sigmaS = avg * 0.02; //Set spatial sigma to equal 2% of viewport size
		
		memcpy(_gpu.compUB, &_postfx.ub, sizeof(GPUCompUB));

		// Reset luminance from previous frame
		memset(_gpu.compSSBO->luminance, 0, sizeof(_gpu.compSSBO->luminance));
	}
}

void Engine::durand2002(VkCommandBuffer& cmd, int imageIndex)
{
	beginCmdDebugLabel(cmd, DURAND_DBG_PREF);
	
	uint32_t groupsX = _viewport.width  / COMPUTE_THREADS_XY + 1;
	uint32_t groupsY = _viewport.height / COMPUTE_THREADS_XY + 1;

	PostFX& cp = _postfx;
	_postfx.Stage(DURAND, "0").Bind(cmd)
		// In
		.UpdateImage(_viewport.imageViews[imageIndex], 1)
		// Out
		.UpdateImage(cp.Att(DURAND, "lum").view, 2)
		.UpdateImage(cp.Att(DURAND, "chrom").view, 3)

		.Dispatch(groupsX, groupsY, imageIndex)
		.Barrier();

	_postfx.Stage(DURAND, "1").Bind(cmd)
		// In
		.UpdateImage(cp.Att(DURAND, "lum").view, 1)
		// Out
		.UpdateImage(cp.Att(DURAND, "base").view, 2)
		.UpdateImage(cp.Att(DURAND, "detail").view, 3)

		.Dispatch(groupsX, groupsY, imageIndex)
		.Barrier();

	_postfx.Stage(DURAND, "2").Bind(cmd)
		// In
		.UpdateImage(cp.Att(DURAND, "lum").view, 1)
		.UpdateImage(cp.Att(DURAND, "chrom").view, 2)
		.UpdateImage(cp.Att(DURAND, "base").view, 3)
		.UpdateImage(cp.Att(DURAND, "detail").view, 4)
		// Out
		.UpdateImage(_viewport.imageViews[imageIndex], 5)

		.Dispatch(groupsX, groupsY, imageIndex)
		.Barrier();

	endCmdDebugLabel(cmd);
}


void Engine::exposureFusion_Downsample(VkCommandBuffer& cmd, int imageIndex, std::string suffix)
{
	PostFX& cp = _postfx;
	beginCmdDebugLabel(cmd, FUSION_DBG_PREF + "::DOWNSAMPLE::" + cpp_utils::str_toupper(suffix));
	{
		cp.Stage(FUSION, "downsample0_"+suffix).Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, suffix), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "downsampled0"), 2)
			.WriteSets(imageIndex);

		cp.Stage(FUSION, "downsample1_"+suffix).Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "downsampled0"), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "downsampled1"), 2)
			.WriteSets(imageIndex);

		cp.Stage(FUSION, "downsample2_"+suffix).Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "downsampled1"), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, suffix), 2)
			.WriteSets(imageIndex);

		int first_i = 0;
		uint32_t w = _viewport.width >> first_i;
		uint32_t h = _viewport.height >> first_i;

		for (int i = first_i; i < _postfx.ub.numOfViewportMips - 1; ++i) {
			beginCmdDebugLabel(cmd, FUSION_DBG_PREF + "::DOWNSAMPLE::" + cpp_utils::str_toupper(suffix) + "::MIP_" + std::to_string(i));

			uint32_t groupsX = w / COMPUTE_THREADS_XY + 1;
			uint32_t groupsY = h / COMPUTE_THREADS_XY + 1;

			GPUCompPC pc = {
				.mipIndex = i,
				.horizontalPass = true
			};
			vkCmdPushConstants(cmd, cp.Stage(FUSION, "downsample0_"+suffix).pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

			cp.Stage(FUSION, "downsample0_"+suffix).Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			pc.horizontalPass = false;
			vkCmdPushConstants(cmd, cp.Stage(FUSION, "downsample1_"+suffix).pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

			cp.Stage(FUSION, "downsample1_"+suffix).Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			// Decimate is run over higher mip so need to change mip index and groups number
			pc.mipIndex = i + 1;
			vkCmdPushConstants(cmd, cp.Stage(FUSION, "downsample2_"+suffix).pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);
			cp.Stage(FUSION, "downsample2_"+suffix).Bind(cmd)
				.Dispatch((w >> 1) / COMPUTE_THREADS_XY + 1, (h >> 1) / COMPUTE_THREADS_XY + 1, imageIndex)
				.Barrier();

			w >>= 1;
			h >>= 1;

			endCmdDebugLabel(cmd);
		}
	}
	endCmdDebugLabel(cmd);
}

void Engine::exposureFusion(VkCommandBuffer& cmd, int imageIndex)
{
	beginCmdDebugLabel(cmd, FUSION_DBG_PREF);
	auto& cp = _postfx;
	beginCmdDebugLabel(cmd, FUSION_DBG_PREF + "::LUM_CHROM_WEIGHT");
	{
		uint32_t groupsX = _viewport.width / COMPUTE_THREADS_XY + 1;
		uint32_t groupsY = _viewport.height / COMPUTE_THREADS_XY + 1;

		cp.Stage(FUSION, "0").Bind(cmd)
			// In
			.UpdateImage(_viewport.imageViews[imageIndex], 1)
			// Out
			.UpdateImage(cp.Att(FUSION, "chrom"), 2)
			.UpdateImage(cp.Pyr(FUSION, "lum").views[0], 3)
			.UpdateImage(cp.Pyr(FUSION, "weight").views[0], 4)

			.Dispatch(groupsX, groupsY, imageIndex)
			.Barrier();
	}
	endCmdDebugLabel(cmd);

	exposureFusion_Downsample(cmd, imageIndex, "lum");
	exposureFusion_Downsample(cmd, imageIndex, "weight");
	
	beginCmdDebugLabel(cmd, FUSION_DBG_PREF + "::LAPLACIANS_MIPS");
	{
		{
			uint32_t groupsX = _viewport.width  / COMPUTE_THREADS_XY + 1;
			uint32_t groupsY = _viewport.height / COMPUTE_THREADS_XY + 1;

			// Move highest lum mip (low-pass residual) to highest laplacian mip
			cp.Stage(FUSION, "move").Bind(cmd)
				// In
				.UpdateImage(cp.Pyr(FUSION, "lum").views[_postfx.ub.numOfViewportMips - 1], 1)
				// Out
				.UpdateImage(cp.Pyr(FUSION, "laplac").views[_postfx.ub.numOfViewportMips - 1], 2)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();
		}


		cp.Stage(FUSION, "upsample0_sub").Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "lum"), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 2)
			.WriteSets(imageIndex);
		// Horiz filter
		cp.Stage(FUSION, "upsample1_sub").Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled1"), 2)
			.WriteSets(imageIndex);
		// Vert filter
		cp.Stage(FUSION, "upsample2_sub").Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled1"), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 2)
			.WriteSets(imageIndex);

		cp.Stage(FUSION, "subtract").Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 1) // Src
			.UpdateImagePyramid(cp.Pyr(FUSION, "lum"), 2) // Value
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "laplac"), 3) // Dst
			.WriteSets(imageIndex);

		int first_i = _postfx.ub.numOfViewportMips - 2;
		uint32_t w = _viewport.width >> first_i;
		uint32_t h = _viewport.height >> first_i;

		for (int i = first_i; i >= 0; --i) {
			beginCmdDebugLabel(cmd, FUSION_DBG_PREF + "::LAPLACIANS_MIPS::MIP_" + std::to_string(i));

			uint32_t groupsX = w / COMPUTE_THREADS_XY + 1;
			uint32_t groupsY = h / COMPUTE_THREADS_XY + 1;

			GPUCompPC pc = {
				.mipIndex = i,
				.horizontalPass = true
			};
			vkCmdPushConstants(cmd, cp.Stage(FUSION, "upsample1_sub").pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

			cp.Stage(FUSION, "upsample0_sub").Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			cp.Stage(FUSION, "upsample1_sub").Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			pc.horizontalPass = false;
			vkCmdPushConstants(cmd, cp.Stage(FUSION, "upsample2_sub").pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

			cp.Stage(FUSION, "upsample2_sub").Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			cp.Stage(FUSION, "subtract").Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			w <<= 1;
			h <<= 1;

			endCmdDebugLabel(cmd);
		}
	}
	endCmdDebugLabel(cmd);

	beginCmdDebugLabel(cmd, FUSION_DBG_PREF + "::BLEND_LAPLACIANS");
	{
		uint32_t groupsX = _viewport.width  / COMPUTE_THREADS_XY + 1;
		uint32_t groupsY = _viewport.height / COMPUTE_THREADS_XY + 1;

		PostFXStage& cs = cp.Stage(FUSION, "2");
		cs.Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "laplac"), 1)
			.UpdateImagePyramid(cp.Pyr(FUSION, "weight"), 2)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "blendedLaplac"), 3)

			.Dispatch(groupsX, groupsY, imageIndex)
			.Barrier();
	}
	endCmdDebugLabel(cmd);

	beginCmdDebugLabel(cmd, FUSION_DBG_PREF + "::SUM_BLENDED_LAPLACIANS");
	{

		cp.Stage(FUSION, "upsample0_add").Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "blendedLaplac"), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 2)
			.WriteSets(imageIndex);

		cp.Stage(FUSION, "upsample1_add").Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled1"), 2)
			.WriteSets(imageIndex);

		cp.Stage(FUSION, "upsample2_add").Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled1"), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 2)
			.WriteSets(imageIndex);

		cp.Stage(FUSION, "add").Bind(cmd)
			// In
			.UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 1)
			// Out
			.UpdateImagePyramid(cp.Pyr(FUSION, "blendedLaplac"), 2)
			.WriteSets(imageIndex);

		// TODO:numMips - 1 and change upsample0.comp instead
		int first_i = _postfx.ub.numOfViewportMips - 2;
		uint32_t w = _viewport.width >> first_i;
		uint32_t h = _viewport.height >> first_i;

		for (int i = first_i; i >= 0; --i) {
			beginCmdDebugLabel(cmd, FUSION_DBG_PREF + "::SUM_BLENDED_LAPLACIANS::MIP_" + std::to_string(i));

			uint32_t groupsX = w / COMPUTE_THREADS_XY + 1;
			uint32_t groupsY = h / COMPUTE_THREADS_XY + 1;

			GPUCompPC pc = {
				.mipIndex = i,
				.horizontalPass = true
			};
			vkCmdPushConstants(cmd, cp.Stage(FUSION, "upsample1_add").pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

			cp.Stage(FUSION, "upsample0_add").Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			cp.Stage(FUSION, "upsample1_add").Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			pc.horizontalPass = false;
			vkCmdPushConstants(cmd, cp.Stage(FUSION, "upsample2_add").pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

			cp.Stage(FUSION, "upsample2_add").Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			cp.Stage(FUSION, "add").Bind(cmd)
				.Dispatch(groupsX, groupsY, imageIndex)
				.Barrier();

			w <<= 1;
			h <<= 1;

			endCmdDebugLabel(cmd);
		}
	}
	endCmdDebugLabel(cmd);

	// Restore color
	beginCmdDebugLabel(cmd, FUSION_DBG_PREF + "::RESTORE_FINAL_COLOR");
	{
		uint32_t groupsX = _viewport.width  / COMPUTE_THREADS_XY + 1;
		uint32_t groupsY = _viewport.height / COMPUTE_THREADS_XY + 1;

		cp.Stage(FUSION, "4").Bind(cmd)
			// In
			.UpdateImage(cp.Att(FUSION, "chrom"), 1)
			//.UpdateImage(_viewport.imageViews[imageIndex], 1)
			//.UpdateImage(cp.Att(FUSION, "laplacSum"), 2)
			.UpdateImage(cp.Pyr(FUSION, "blendedLaplac").views[0], 2)
			// Out
			.UpdateImage(_viewport.imageViews[imageIndex], 3)

			.Dispatch(groupsX, groupsY, imageIndex)
			.Barrier();
	}
	endCmdDebugLabel(cmd);

	endCmdDebugLabel(cmd); // end of LTM::FUSION
}


void Engine::shadowPass(FrameData& f, int imageIndex)
{
	beginCmdDebugLabel(f.cmd, "SHADOW_PASS");
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
			updateShadowCubemapFace(f, i, face);

		}
		anyMoved = true;
	}

	if (anyMoved) {
		// Wait for shadow cubemap array to render before rendering the scene
		vk_utils::imageMemoryBarrier(f.cmd, _shadow.cubemapArray.allocImage.image,
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			VK_ACCESS_SHADER_READ_BIT,

			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

			FULL_COLOR_RANGE);
	}

	endCmdDebugLabel(f.cmd);
}

void Engine::viewportPass(VkCommandBuffer& cmd, int imageIndex)
{
	cmdSetViewportScissor(cmd, _viewport.width, _viewport.height);
	{ // Viewport pass
		// Translate to required layout
		vk_utils::imageMemoryBarrier(cmd, _viewport.images[imageIndex].image,
			0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

			FULL_COLOR_RANGE);

		VkRenderingAttachmentInfo colorAttachmentInfo = vkinit::rendering_attachment_info(_viewport.imageViews[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		VkRenderingAttachmentInfo depthAttachmentInfo = vkinit::rendering_attachment_info(_viewport.depth.view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

		VkRenderingInfo renderingInfo = vkinit::rendering_info(&colorAttachmentInfo, &depthAttachmentInfo, _viewport.width, _viewport.height);

		// Viewport pass
		beginCmdDebugLabel(cmd, "VIEWPORT_PASS");
		vkCmdBeginRendering(cmd, &renderingInfo);
		{
			drawObjects(cmd, _renderables);
		}
		vkCmdEndRendering(cmd);
		endCmdDebugLabel(cmd);
	}
}


void Engine::postfxPass(FrameData& f, int imageIndex)
{
	beginCmdDebugLabel(f.cmd, "POSTFX_PASS");

	PostFX& cp = _postfx;

	glm::uvec2 div16 = glm::uvec2(_viewport.width >> 4, _viewport.height >> 4);
	glm::uvec2 div32 = glm::uvec2(div16.x >> 1, div16.y >> 1);

	glm::uvec2 groups16 = div16 + glm::uvec2(1);
	glm::uvec2 groups32 = div32 + glm::uvec2(1);

	if (_postfx.enableAdaptation) {
		beginCmdDebugLabel(f.cmd, "EYE_ADAPTATION");
		// Compute luminance histogram
		ASSERT(MAX_LUMINANCE_BINS == 256);

		cp.Stage(EXPADP, "histogram").Bind(f.cmd)
			.UpdateImage(_viewport.imageViews[imageIndex], 2)
			.Dispatch(groups16.x, groups16.y, imageIndex)
			.Barrier();

		// Compute average luminance
		cp.Stage(EXPADP, "avglum").Bind(f.cmd)
			.Dispatch(1, 1, imageIndex)
			.Barrier();

		cp.Stage(EXPADP, "eyeadp").Bind(f.cmd)
			.UpdateImage(_viewport.imageViews[imageIndex], 1)
			.Dispatch(groups32.x, groups32.y, imageIndex)
			.Barrier();

		endCmdDebugLabel(f.cmd);
	}

	if (_postfx.enableBloom) {
		beginCmdDebugLabel(f.cmd, "BLOOM");
		{
			cp.Stage(BLOOM, "threshold").Bind(f.cmd)
				.UpdateImage(_viewport.imageViews[imageIndex], 1)
				.UpdateImage(cp.Pyr(BLOOM, "highlights").views[0], 2)
				.Dispatch(groups32.x, groups32.y, imageIndex)
				.Barrier();

			//const int blur_times = _renderContext.numOfBloomBlurPasses * 2;
			//ASSERT(blur_times % 2 != 1);

			cp.Stage(BLOOM, "downsample").Bind(f.cmd)
				.UpdateImagePyramid(cp.Pyr(BLOOM, "highlights"), 0)
				.WriteSets(imageIndex);

			cp.Stage(BLOOM, "blur0").Bind(f.cmd)
				.UpdateImagePyramid(cp.Pyr(BLOOM, "highlights"), 0)
				.UpdateImagePyramid(cp.Pyr(BLOOM, "blur"), 1)
				.WriteSets(imageIndex);

			cp.Stage(BLOOM, "blur1").Bind(f.cmd)
				.UpdateImagePyramid(cp.Pyr(BLOOM, "blur"), 0)
				.UpdateImagePyramid(cp.Pyr(BLOOM, "highlights"), 1)
				.WriteSets(imageIndex);

			cp.Stage(BLOOM, "upsample").Bind(f.cmd)
				.UpdateImagePyramid(cp.Pyr(BLOOM, "highlights"), 0)
				.WriteSets(imageIndex);


			GPUCompPC pc = {};

			beginCmdDebugLabel(f.cmd, "BLOOM::DOWNSAMPLE_AND_BLUR");
			for (int i = 1; i < _postfx.numOfBloomMips; ++i) {
				glm::uvec2 groups32 = glm::uvec2(_viewport.width >> i >> 5, _viewport.height >> i >> 5) + glm::uvec2(1);

				pc.mipIndex = i;
				pc.horizontalPass = true;
				vkCmdPushConstants(f.cmd, cp.Stage(BLOOM, "downsample").pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

				// Downsample
				cp.Stage(BLOOM, "downsample").Bind(f.cmd)
					.Dispatch(groups32.x, groups32.y, imageIndex)
					.Barrier();
#if 0
				pc.mipIndex = i;
				pc.horizontalPass = true;
				vkCmdPushConstants(f.cmd, cp.Stage(BLOOM, "blur0").pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

				cp.Stage(BLOOM, "blur0").Bind(f.cmd)
					.Dispatch(groups32.x, groups32.y, imageIndex)
					.Barrier();

				pc.mipIndex = i;
				pc.horizontalPass = false;
				vkCmdPushConstants(f.cmd, cp.Stage(BLOOM, "blur1").pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

				cp.Stage(BLOOM, "blur1").Bind(f.cmd)
					.Dispatch(groups32.x, groups32.y, imageIndex)
					.Barrier();
#endif
			}
			endCmdDebugLabel(f.cmd);
#if 0
			beginCmdDebugLabel(f.cmd, "BLOOM::UPSAMPLE_AND_ADD");
			for (int i = _postfx.numOfBloomMips - 1; i >= 1; --i) {
				glm::uvec2 groups32 = glm::uvec2(_viewport.width >> i >> 5, _viewport.height >> i >> 5) + glm::uvec2(1);

				pc.mipIndex = i;
				vkCmdPushConstants(f.cmd, cp.Stage(BLOOM, "upsample").pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(GPUCompPC), &pc);

				cp.Stage(BLOOM, "upsample").Bind(f.cmd)
					.Dispatch(groups32.x, groups32.y, imageIndex)
					.Barrier();
			}
			endCmdDebugLabel(f.cmd);


			cp.Stage(BLOOM, "combine").Bind(f.cmd)
				.UpdateImage(_viewport.imageViews[imageIndex], 1)
				.UpdateImage(cp.Pyr(BLOOM, "highlights").views[0], 2)
				.Dispatch(groups32.x, groups32.y, imageIndex)
				.Barrier();
#endif
		}
		endCmdDebugLabel(f.cmd);
	}



	if (_postfx.enableLocalToneMapping) {
		// A read & write compute barrier to avoid WRITE_AFTER_WRITE hazards between queue submits
		vk_utils::memoryBarrier(f.cmd,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

		if (_postfx.localToneMappingMode == PostFX::LTM::DURAND) {
			durand2002(f.cmd, imageIndex);
		} else if (_postfx.localToneMappingMode == PostFX::LTM::FUSION) {
			exposureFusion(f.cmd, imageIndex);
		} else {
			ASSERT(false);
		}

		// A read & write compute barrier to avoid WRITE_AFTER_WRITE hazards between queue submits
		vk_utils::memoryBarrier(f.cmd,
			VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
	}

	if (_postfx.enableGlobalToneMapping) {
		beginCmdDebugLabel(f.cmd, "GLOBAL_TONE_MAPPING");

		cp.Stage(GTMO, "0").Bind(f.cmd)
			.UpdateImage(_viewport.imageViews[imageIndex], 1)
			.Dispatch(groups32.x, groups32.y, imageIndex);

		endCmdDebugLabel(f.cmd);
	}

	if (_postfx.enableGammaCorrection) {
		beginCmdDebugLabel(f.cmd, "GAMMA_CORRECTION");

		cp.Stage(GAMMA, "0").Bind(f.cmd)
			.UpdateImage(_viewport.imageViews[imageIndex], 1)
			.Dispatch(groups32.x, groups32.y, imageIndex);

		endCmdDebugLabel(f.cmd);
	}
	endCmdDebugLabel(f.cmd); // end of compute
}

void Engine::swapchainPass(FrameData& f, int imageIndex)
{
	beginCmdDebugLabel(f.cmd, "SWAPCHAIN_PASS");

	VkRenderingAttachmentInfo colorAttachmentInfo = vkinit::rendering_attachment_info(_swapchain.imageViews[imageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderingInfo = vkinit::rendering_info(&colorAttachmentInfo, nullptr, _swapchain.width, _swapchain.height);

	vkCmdBeginRendering(f.cmd, &renderingInfo);
	{
		// Record dear imgui primitives into command buffer
		ui_OnRenderPassEnd(f.cmd);
	}
	vkCmdEndRendering(f.cmd);

	endCmdDebugLabel(f.cmd);
}


void Engine::recordCommandBuffer(FrameData& f, uint32_t imageIndex)
{
	loadDataToGPU();

	shadowPass(f, imageIndex);

	viewportPass(f.cmd, imageIndex);

	// Need to wait until scene is rendered to proceed with post-processing
	vk_utils::imageMemoryBarrier(f.cmd, _viewport.images[imageIndex].image,
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,

		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,

		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,

		FULL_COLOR_RANGE);

	postfxPass(f, imageIndex);

	// Post-processing must finish because viewport image is sampled from during swapchain pass
	vk_utils::imageMemoryBarrier(f.cmd, _viewport.images[imageIndex].image,
		VK_ACCESS_SHADER_WRITE_BIT,
		VK_ACCESS_SHADER_READ_BIT,

		VK_IMAGE_LAYOUT_GENERAL,
		//VK_IMAGE_LAYOUT_GENERAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,

		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

		FULL_COLOR_RANGE);

	// Traslate to required layout
	vk_utils::imageMemoryBarrier(f.cmd, _swapchain.images[imageIndex],
		0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,

		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,

		//VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, // Why doesn't this work?
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

		FULL_COLOR_RANGE);

	swapchainPass(f, imageIndex);

	// Translate to required layout for presentation on screen
	vk_utils::imageMemoryBarrier(f.cmd, _swapchain.images[imageIndex],
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,

		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,

		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,

		FULL_COLOR_RANGE);
}

void Engine::drawFrame()
{
	FrameData& f = _frames[_frameInFlightNum];

	// Since we have descriptor set copies for each frame in flight,
	// we set the pointers to current frame's descriptor set buffers
	_gpu.Reset(f);
	
	// Needs to be part of drawFrame because drawFrame is called from onFramebufferResize callback
	ui_Update();
	ui_OnDrawStart();

    //Get index of next image after 
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_device, _swapchain.handle, UINT64_MAX, f.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    {
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            return;
        }
    }

	VK_ASSERT(vkWaitForFences(_device, 1, &f.inFlightFence, VK_TRUE, UINT64_MAX));
	VK_ASSERT(vkResetFences(_device, 1, &f.inFlightFence));
	{ // Command buffer
		VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info();

		VK_ASSERT(vkBeginCommandBuffer(f.cmd, &beginInfo));
		{
			recordCommandBuffer(f, imageIndex);
		}
		VK_ASSERT(vkEndCommandBuffer(f.cmd));
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

    VK_ASSERT(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, f.inFlightFence));

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
        VK_ASSERTMSG(result, "failed to present swap chain image!");
    }

	_frameInFlightNum = (_frameInFlightNum + 1) % MAX_FRAMES_IN_FLIGHT;
}

