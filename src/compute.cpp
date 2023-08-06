#include "stdafx.h"
#include "engine.h"

#include "compute.h"

#include "vk_descriptors.h"




void ComputeStage::Create(VkDevice device, VkSampler sampler,
	const std::string& shaderBinName, bool usePushConstants/* = false*/)
{
	this->device = device;
	this->sampler = sampler;

	ShaderData comp;
	comp.code = vk_utils::readShaderBinary(Engine::SHADER_PATH + shaderBinName);

	if (vk_utils::createShaderModule(device, comp.code, &comp.module)) {
		std::cout << "Compute shader successfully loaded." << std::endl;
	} else {
		PRWRN("Failed to load compute shader");
	}

	VkPipelineShaderStageCreateInfo stageInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = comp.module,
		.pName = "main"
	};

	VkPipelineLayoutCreateInfo layoutInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &setLayout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr
	};

	if (usePushConstants) {
		VkPushConstantRange pcRange = {
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.offset = 0,
			.size = sizeof(GPUCompPC),
		};

		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pcRange;
	}

	VK_ASSERT(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

	VkComputePipelineCreateInfo computePipelineInfo{
		.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
		.stage = stageInfo,
		.layout = pipelineLayout,
	};

	VK_ASSERT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &pipeline));

	vkDestroyShaderModule(device, comp.module, nullptr);

	imageBindings.reserve(MAX_IMAGE_UPDATES);
}

ComputeStage& ComputeStage::Bind(VkCommandBuffer cmd)
{
	commandBuffer = cmd;
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	return *this;
}

ComputeStage& ComputeStage::UpdateImage(VkImageView view, uint32_t binding)
{
	imageBindings.push_back({ { view }, binding });
	ASSERT(imageBindings.size() <= MAX_IMAGE_UPDATES);

	return *this;
}

ComputeStage& ComputeStage::UpdateImage(Attachment att, uint32_t binding)
{
	imageBindings.push_back({ { att.view }, binding });
	ASSERT(imageBindings.size() <= MAX_IMAGE_UPDATES);

	return *this;
}

ComputeStage& ComputeStage::UpdateImagePyramid(AttachmentPyramid& att, uint32_t binding)
{
	imageBindings.push_back({ att.views, binding });
	ASSERT(imageBindings.size() <= MAX_IMAGE_UPDATES);

	return *this;
}

ComputeStage& ComputeStage::WriteSets(int set_i)
{
	if (!imageBindings.empty()) {
		// Need to create vector for image infos to hold data until update command is executed
		std::vector<std::vector<VkDescriptorImageInfo>> imageInfos;
		imageInfos.resize(imageBindings.size());

		std::vector<VkWriteDescriptorSet> writes;
		int i = 0;
		for (auto& ib : imageBindings) {
			for (auto& v : ib.views) {
				VkDescriptorImageInfo imgInfo{
					.sampler = sampler,
					.imageView = v,
					.imageLayout = VK_IMAGE_LAYOUT_GENERAL
				};

				imageInfos[i].push_back(imgInfo);
			}

			VkWriteDescriptorSet write = {};
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.pNext = nullptr;

			write.dstBinding = ib.binding;
			write.dstSet = sets[set_i];
			write.descriptorCount = imageInfos[i].size();
			write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			write.pImageInfo = imageInfos[i].data();

			writes.push_back(write);
			++i;
		}

		vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
		imageBindings.clear();
	}

	return *this;
}

ComputeStage& ComputeStage::Dispatch(uint32_t groupsX, uint32_t groupsY, int set_i)
{
	WriteSets(set_i);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &sets[set_i], 0, nullptr);
	vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);

	return *this;
}

void ComputeStage::Barrier()
{
	vk_utils::memoryBarrier(commandBuffer,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void ComputeStage::InitDescriptorSets(
	DescriptorLayoutCache* dLayoutCache, DescriptorAllocator* dAllocator,
	FrameData& f, int frame_i)
{
	ASSERT(!dsetBindings.empty());

	DescriptorBuilder builder = DescriptorBuilder::begin(dLayoutCache, dAllocator);

	for (int i = 0; i < dsetBindings.size(); ++i) {
		switch (dsetBindings[i]) {
		case UB:
			builder.bind_buffer(i, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
			break;
		case SSBO:
			builder.bind_buffer(i, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
			break;
		case IMG:
			builder.bind_image_empty(i, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
			break;
		case PYR:
			builder.bind_image_empty(i, MAX_VIEWPORT_MIPS, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
			break;
		default:
			ASSERT(false);
			break;
		}
	}

	builder.build(sets[frame_i], setLayout);
}

void ComputeStage::Destroy()
{
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
}

Durand2002::Durand2002()
{
	stages["0"] = { .shaderName = "ltm_durand_lum_chrom.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG} };
	stages["1"] = { .shaderName = "ltm_durand_bilateral_base.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG } };
	stages["2"] = { .shaderName = "ltm_durand_reconstruct.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG, IMG, IMG } };

	att["lum"] = att["chrom"] = att["base"] = att["detail"] = {};
}

ExposureFusion::ExposureFusion()
{
	stages["0"] = { .shaderName = "ltm_fusion_0.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG, IMG} };
	stages["1"] = { .shaderName = "ltm_fusion_1.comp.spv", .dsetBindings = { UB, PYR, PYR }, .usesPushConstants = true };
	stages["2"] = { .shaderName = "ltm_fusion_2.comp.spv", .dsetBindings = { UB, PYR, PYR, PYR } };
	//stages["3"] = { .shaderName = "ltm_fusion_3.comp.spv", .dsetBindings = { UB, PYR, IMG} };
	stages["4"] = { .shaderName = "ltm_fusion_4.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG} };

	// TODO: change naming to "_filter_horiz/vert"
	stages["upsample0_sub"] = { .shaderName = "upsample.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["upsample1_sub"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["upsample2_sub"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };

	stages["upsample0_add"] = { .shaderName = "upsample.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["upsample1_add"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["upsample2_add"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };

	stages["add"] = { .shaderName = "mipmap_add.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["subtract"] = { .shaderName = "mipmap_subtract.comp.spv", .dsetBindings = { UB, PYR, PYR, PYR}, .usesPushConstants = true };

	stages["move"] = { .shaderName = "move.comp.spv", .dsetBindings = { UB, IMG, IMG } };

	// TODO: change naming to "_filter_horiz/vert"
	stages["downsample0_lum"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["downsample1_lum"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["downsample2_lum"] = { .shaderName = "decimate_new.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };

	stages["downsample0_weight"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["downsample1_weight"] = { .shaderName = "filter.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };
	stages["downsample2_weight"] = { .shaderName = "decimate_new.comp.spv", .dsetBindings = { UB, PYR, PYR}, .usesPushConstants = true };

	att["chrom"] = att["laplacSum"] = {};

	pyr["lum"] = pyr["weight"] = pyr["laplac"] = pyr["blendedLaplac"] = {};
	// Pyramid that will hold intermediate filtered images when upsampling
	pyr["upsampled0"] = pyr["upsampled1"] = {};
	// Pyramid that will hold intermediate filtered images when downsampling
	pyr["downsampled0"] = pyr["downsampled1"] = {};
}

Bloom::Bloom()
{
	stages["0"] = { .shaderName = "bloom_threshold.comp.spv", .dsetBindings = { UB, IMG, IMG } };

	stages["blur0"] = { .shaderName = "bloom_blur.comp.spv", .dsetBindings = { UB, IMG, IMG }, .usesPushConstants = true };
	stages["blur1"] = { .shaderName = "bloom_blur.comp.spv", .dsetBindings = { UB, IMG, IMG }, .usesPushConstants = true };

	stages["2"] = { .shaderName = "bloom_combine.comp.spv", .dsetBindings = { UB, IMG, IMG } };

	att["highlights"] = att["blur0"] = att["blur1"] = {};
}
