#include "stdafx.h"
#include "engine.h"

#include "postfx.h"

#include "vk_descriptors.h"

PostFX::PostFX()
{
	numOfBloomMips = 5;

	lumPixelLowerBound = 0.2f;
	lumPixelUpperBound = 0.95f;

	maxLogLuminance = 4.7f;
	eyeAdaptationTimeCoefficient = 2.2f;

	localToneMappingMode = LTM::DURAND; // 0 - Durand2002, 1 - Exposure fusion

	gamma = 2.2f;
	gammaMode = 0; // 0 - forward, 1 - inverse

	enableBloom = true;
	enableGlobalToneMapping = false;
	enableGammaCorrection = false;
	enableAdaptation = false;
	enableLocalToneMapping = false;

	effectPrefixMap[EXPADP] = "expadp_";
	effectPrefixMap[DURAND] = "durand_";
	effectPrefixMap[FUSION] = "fusion_";
	effectPrefixMap[BLOOM] = "bloom_";
	effectPrefixMap[GTMO] = "gtmo_";
	effectPrefixMap[GAMMA] = "gamma_";

	std::string pref = "";
	{
		pref = getPrefixFromEffect(DURAND);
		stages[pref + "lum_chrom"] = { .shaderName = "ltm_durand_lum_chrom.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG} };
		stages[pref + "bilateral"] = { .shaderName = "ltm_durand_bilateral.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG } };
		stages[pref + "reconstruct"] = { .shaderName = "ltm_durand_reconstruct.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG, IMG } };

		att[pref + "lum"] = {};
		att[pref + "chrom"] = {};
		att[pref + "base"] = {};
		att[pref + "detail"] = {};
	}
	{
		pref = getPrefixFromEffect(FUSION);
		stages[pref + "lum_chrom_weight"] = { .shaderName = "ltm_fusion_lum_chrom_weight.comp.spv", .dsetBindings = { UB, IMG, IMG, IMG, IMG} };


		stages[pref + "upsample0_sub"] = { .shaderName = "ltm_fusion_upsample.comp.spv", .dsetBindings = { PYR, PYR }, .usesPushConstants = true };
		stages[pref + "upsample1_sub"] = { .shaderName = "ltm_fusion_laplacian.comp.spv", .dsetBindings = { PYR, PYR, PYR, PYR }, .usesPushConstants = true };

		stages[pref + "residual"] = { .shaderName = "ltm_fusion_residual.comp.spv", .dsetBindings = { IMG, IMG, IMG } };

		stages[pref + "upsample0_add"] = { .shaderName = "ltm_fusion_upsample.comp.spv", .dsetBindings = { PYR, PYR}, .usesPushConstants = true };
		stages[pref + "upsample1_add"] = { .shaderName = "ltm_fusion_blended_laplacian_sum.comp.spv", .dsetBindings = { PYR, PYR}, .usesPushConstants = true };

		stages[pref + "reconstruct"] = { .shaderName = "ltm_fusion_reconstruct.comp.spv", .dsetBindings = { IMG, IMG, IMG} };

		stages[pref + "downsample"] = { .shaderName = "ltm_fusion_downsample.comp.spv", .dsetBindings = { PYR, PYR }, .usesPushConstants = true };

		att[pref + "chrom"] = {};

		pyr[pref + "lum"] = pyr[pref + "weight"] = pyr[pref + "blendedLaplac"] = {};
		// Pyramid that will hold intermediate filtered images when upsampling
		pyr[pref + "upsampled0"] = {};
	}
	{
		pref = getPrefixFromEffect(BLOOM);
		stages[pref + "threshold"] = { .shaderName = "bloom_threshold.comp.spv", .dsetBindings = { UB, IMG, IMG } };
		stages[pref + "downsample"] = { .shaderName = "bloom_downsample.comp.spv", .dsetBindings = { UB, PYR }, .usesPushConstants = true };

		stages[pref + "upsample"] = { .shaderName = "bloom_upsample.comp.spv", .dsetBindings = { UB, PYR }, .usesPushConstants = true };
		stages[pref + "combine"] = { .shaderName = "bloom_combine.comp.spv", .dsetBindings = { UB, IMG, IMG } };

		pyr[pref + "highlights"] = {};
	}
	{
		pref = getPrefixFromEffect(EXPADP);
		stages[pref + "histogram"] = { .shaderName = "expadp_histogram.comp.spv", .dsetBindings = { SSBO, UB, IMG } };
		stages[pref + "avglum"] = { .shaderName = "expadp_average_luminance.comp.spv", .dsetBindings = { SSBO, UB } };
		stages[pref + "eyeadp"] = { .shaderName = "expadp_eye_adaptation.comp.spv", .dsetBindings = { SSBO, IMG } };
	}
	{
		pref = getPrefixFromEffect(GTMO);
		stages[pref + "0"] = { .shaderName = "global_tone_mapping.comp.spv", .dsetBindings = { UB, IMG } };
	}
	{
		pref = getPrefixFromEffect(GAMMA);
		stages[pref + "0"] = { .shaderName = "gamma_correction.comp.spv", .dsetBindings = { UB, IMG } };
	}
}

void PostFX::UpdateStagesDescriptorSets(int numOfFrames, const std::vector<VkImageView>& viewportImageViews)
{
	for (int i = 0; i < numOfFrames; ++i) {
		int imageIndex = i;

		{ // Durand
			Stage(DURAND, "lum_chrom")
				// In
				.UpdateImage(viewportImageViews[imageIndex], 1)
				// Out
				.UpdateImage(Att(DURAND, "lum").view, 2)
				.UpdateImage(Att(DURAND, "chrom").view, 3)

				.WriteSets(imageIndex);

			Stage(DURAND, "bilateral")
				// In
				.UpdateImage(Att(DURAND, "lum").view, 1)
				// Out
				.UpdateImage(Att(DURAND, "base").view, 2)
				.UpdateImage(Att(DURAND, "detail").view, 3)

				.WriteSets(imageIndex);

			Stage(DURAND, "reconstruct")
				// In
				.UpdateImage(Att(DURAND, "chrom").view, 1)
				.UpdateImage(Att(DURAND, "base").view, 2)
				.UpdateImage(Att(DURAND, "detail").view, 3)
				// Out
				.UpdateImage(viewportImageViews[imageIndex], 4)

				.WriteSets(imageIndex);
		}

		{ // Bloom
			Stage(BLOOM, "threshold")
				.UpdateImage(viewportImageViews[imageIndex], 1)
				.UpdateImage(Pyr(BLOOM, "highlights").views[0], 2)
				.WriteSets(imageIndex);

			Stage(BLOOM, "downsample")
				.UpdateImagePyramid(Pyr(BLOOM, "highlights"), 1)
				.WriteSets(imageIndex);

			Stage(BLOOM, "upsample")
				.UpdateImagePyramid(Pyr(BLOOM, "highlights"), 1)
				.WriteSets(imageIndex);

			Stage(BLOOM, "combine")
				.UpdateImage(viewportImageViews[imageIndex], 1)
				.UpdateImage(Pyr(BLOOM, "highlights").views[0], 2)
				.WriteSets(imageIndex);
		}

		{ // Exposure fusion
			Stage(FUSION, "lum_chrom_weight")
				// Out
				.UpdateImage(viewportImageViews[i], 1)
				.UpdateImage(Att(FUSION, "chrom"), 2)
				.UpdateImage(Pyr(FUSION, "lum").views[0], 3)
				.UpdateImage(Pyr(FUSION, "weight").views[0], 4)
				.WriteSets(imageIndex);

			Stage(FUSION, "downsample")
				// In
				.UpdateImagePyramid(Pyr(FUSION, "lum"), 0)
				// Out
				.UpdateImagePyramid(Pyr(FUSION, "weight"), 1)
				.WriteSets(imageIndex);


			int last_i = ub.numOfViewportMips - 1;
			Stage(FUSION, "residual")
				// In
				.UpdateImage(Pyr(FUSION, "lum").views[last_i], 0)
				.UpdateImage(Pyr(FUSION, "weight").views[last_i], 1)
				// Out
				.UpdateImage(Pyr(FUSION, "blendedLaplac").views[last_i], 2)
				.WriteSets(imageIndex);


			Stage(FUSION, "upsample0_sub")
				// In
				.UpdateImagePyramid(Pyr(FUSION, "lum"), 0)
				// Out
				.UpdateImagePyramid(Pyr(FUSION, "upsampled0"), 1)

				.WriteSets(imageIndex);

			Stage(FUSION, "upsample1_sub")
				// In
				.UpdateImagePyramid(Pyr(FUSION, "upsampled0"), 0)
				.UpdateImagePyramid(Pyr(FUSION, "lum"), 1)
				.UpdateImagePyramid(Pyr(FUSION, "weight"), 2)
				// Out
				.UpdateImagePyramid(Pyr(FUSION, "blendedLaplac"), 3)
				.WriteSets(imageIndex);

			Stage(FUSION, "upsample0_add")
				// In
				.UpdateImagePyramid(Pyr(FUSION, "blendedLaplac"), 0)
				// Out
				.UpdateImagePyramid(Pyr(FUSION, "upsampled0"), 1)
				.WriteSets(imageIndex);

			Stage(FUSION, "upsample1_add")
				// In
				.UpdateImagePyramid(Pyr(FUSION, "upsampled0"), 0)
				// Out
				.UpdateImagePyramid(Pyr(FUSION, "blendedLaplac"), 1)
				.WriteSets(imageIndex);

			Stage(FUSION, "reconstruct")
				// In
				.UpdateImage(Att(FUSION, "chrom"), 0)

				.UpdateImage(Pyr(FUSION, "blendedLaplac").views[0], 1)
				// Out
				.UpdateImage(viewportImageViews[imageIndex], 2)

				.WriteSets(imageIndex);
		}

		{ // Exposure adaptation
			Stage(EXPADP, "histogram")
				.UpdateImage(viewportImageViews[imageIndex], 2)
				.WriteSets(imageIndex);

			Stage(EXPADP, "eyeadp")
				.UpdateImage(viewportImageViews[imageIndex], 1)
				.WriteSets(imageIndex);
		}

		{ // Global tone mapping
			Stage(GTMO, "0")
				.UpdateImage(viewportImageViews[imageIndex], 1)
				.WriteSets(imageIndex);
		}

		{ // Gamma correction
			Stage(GAMMA, "0")
				.UpdateImage(viewportImageViews[imageIndex], 1)
				.WriteSets(imageIndex);
		}
	}
}

void PostFXStage::Create(VkDevice device, VkSampler sampler,
	const std::string& shaderBinName, bool usePushConstants/* = false*/)
{
	this->device = device;
	this->sampler = sampler;

	ShaderData comp;
	comp.code = vk_utils::readShaderBinary(SHADER_PATH + shaderBinName);

	if (vk_utils::createShaderModule(device, comp.code, &comp.module)) {
		std::cout << "Compute shader [" << shaderBinName << "] successfully loaded." << std::endl;
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

PostFXStage& PostFXStage::Bind(VkCommandBuffer cmd)
{
	commandBuffer = cmd;
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	return *this;
}

PostFXStage& PostFXStage::UpdateImage(VkImageView view, uint32_t binding)
{
	imageBindings.push_back({ { view }, binding });
	ASSERT(imageBindings.size() <= MAX_IMAGE_UPDATES);

	return *this;
}

PostFXStage& PostFXStage::UpdateImage(Attachment att, uint32_t binding)
{
	imageBindings.push_back({ { att.view }, binding });
	ASSERT(imageBindings.size() <= MAX_IMAGE_UPDATES);

	return *this;
}

PostFXStage& PostFXStage::UpdateImagePyramid(AttachmentPyramid& att, uint32_t binding)
{
	imageBindings.push_back({ att.views, binding });
	ASSERT(imageBindings.size() <= MAX_IMAGE_UPDATES);

	return *this;
}

PostFXStage& PostFXStage::WriteSets(int set_i)
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

PostFXStage& PostFXStage::Dispatch(uint32_t groupsX, uint32_t groupsY, int set_i)
{
	WriteSets(set_i);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &sets[set_i], 0, nullptr);
	vkCmdDispatch(commandBuffer, groupsX, groupsY, 1);

	return *this;
}

void PostFXStage::Barrier()
{
	vk_utils::memoryBarrier(commandBuffer,
		VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
}

void PostFXStage::InitDescriptorSets(
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
			builder.bind_buffer(i, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
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


void PostFXStage::Destroy()
{
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyPipeline(device, pipeline, nullptr);
}



PostFXStage& PostFX::Stage(Effect fct, std::string key) {
	std::string _key = getPrefixFromEffect(fct) + key;
	ASSERT(stages.contains(_key));
	return stages[_key];
}

Attachment& PostFX::Att(Effect fct, std::string key) {
	std::string _key = getPrefixFromEffect(fct) + key;
	ASSERT(att.contains(_key));
	return att[_key];
}

AttachmentPyramid& PostFX::Pyr(Effect fct, std::string key) {
	std::string _key = getPrefixFromEffect(fct) + key;
	ASSERT(pyr.contains(_key));
	return pyr[_key];
}

std::string PostFX::getPrefixFromEffect(Effect fct) {
	switch (fct) {
	case DURAND: case FUSION: case BLOOM: case EXPADP: case GTMO: case GAMMA:
		return effectPrefixMap[fct];
	default:
		ASSERT(false);
		return "";
	}
}

Effect PostFX::getEffectFromPrefix(std::string pref)
{
	for (auto& ep : effectPrefixMap) {
		if (ep.second == pref) {
			return ep.first;
		}
	}
	ASSERT(false);
	return INVALID;
}

bool PostFX::isEffectEnabled(Effect fct)
{
	switch (fct) {
	case DURAND: 
		return enableLocalToneMapping && localToneMappingMode == LTM::DURAND;
	case FUSION:
		return enableLocalToneMapping && localToneMappingMode == LTM::FUSION;
	case BLOOM: 
		return enableBloom;
	case EXPADP: 
		return enableAdaptation;
	case GTMO: 
		return enableGlobalToneMapping;
	case GAMMA:
		return enableGammaCorrection;
	default:
		ASSERT(false);
		return "";
	}
}
