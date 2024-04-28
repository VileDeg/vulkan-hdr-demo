/*The MIT License (MIT)

Copyright (c) 2016 Patrick Marsceill

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.*/

/* Repository: https://github.com/vblanco20-1/vulkan-guide/tree/all-chapters */

/* Initial code was modified */

#include "stdafx.h"
#include "defs.h"
#include "vk_initializers.h"

namespace vkinit {
	VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags /*= 0*/)
	{
		VkCommandPoolCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		info.pNext = nullptr;
		info.queueFamilyIndex = queueFamilyIndex;
		info.flags = flags;
		return info;
	}

	VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count /*= 1*/, VkCommandBufferLevel level /*= VK_COMMAND_BUFFER_LEVEL_PRIMARY*/)
	{
		VkCommandBufferAllocateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		info.pNext = nullptr;

		info.commandPool = pool;
		info.commandBufferCount = count;
		info.level = level;
		return info;
	}

	VkCommandBufferBeginInfo command_buffer_begin_info(VkCommandBufferUsageFlags flags /*= 0*/)
	{
		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		info.pNext = nullptr;

		info.pInheritanceInfo = nullptr;
		info.flags = flags;
		return info;
	}

	VkFramebufferCreateInfo framebuffer_create_info(VkRenderPass renderPass, VkExtent2D extent)
	{
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.pNext = nullptr;

		info.renderPass = renderPass;
		info.attachmentCount = 1;
		info.width = extent.width;
		info.height = extent.height;
		info.layers = 1;

		return info;
	}

	VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags /*= 0*/)
	{
		VkFenceCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		info.pNext = nullptr;

		info.flags = flags;

		return info;
	}

	VkSemaphoreCreateInfo semaphore_create_info(VkSemaphoreCreateFlags flags /*= 0*/)
	{
		VkSemaphoreCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = flags;
		return info;
	}

	VkSubmitInfo submit_info(VkCommandBuffer* cmd)
	{
		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.pNext = nullptr;

		info.waitSemaphoreCount = 0;
		info.pWaitSemaphores = nullptr;
		info.pWaitDstStageMask = nullptr;
		info.commandBufferCount = 1;
		info.pCommandBuffers = cmd;
		info.signalSemaphoreCount = 0;
		info.pSignalSemaphores = nullptr;

		return info;
	}

	VkPresentInfoKHR present_info()
	{
		VkPresentInfoKHR info = {};
		info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		info.pNext = nullptr;

		info.swapchainCount = 0;
		info.pSwapchains = nullptr;
		info.pWaitSemaphores = nullptr;
		info.waitSemaphoreCount = 0;
		info.pImageIndices = nullptr;

		return info;
	}

	VkRenderPassBeginInfo renderpass_begin_info(VkRenderPass renderPass, VkExtent2D windowExtent, VkFramebuffer framebuffer)
	{
		VkRenderPassBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		info.pNext = nullptr;

		info.renderPass = renderPass;
		info.renderArea.offset.x = 0;
		info.renderArea.offset.y = 0;
		info.renderArea.extent = windowExtent;
		info.clearValueCount = 1;
		info.pClearValues = nullptr;
		info.framebuffer = framebuffer;

		return info;
	}

	VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule)
	{
		VkPipelineShaderStageCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		info.pNext = nullptr;

		//shader stage
		info.stage = stage;
		//module containing the code for this shader stage
		info.module = shaderModule;
		//the entry point of the shader
		info.pName = "main";
		return info;
	}

	VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info(
		uint32_t bindingCount, VkVertexInputBindingDescription* pbindings,
		uint32_t attributeCount, VkVertexInputAttributeDescription* pattributes)
	{
		return {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = bindingCount,
			.pVertexBindingDescriptions = pbindings,
			.vertexAttributeDescriptionCount = attributeCount,
			.pVertexAttributeDescriptions = pattributes
		};
	}

	VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology) {
		VkPipelineInputAssemblyStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.topology = topology;
		//we are not going to use primitive restart on the entire tutorial so leave it on false
		info.primitiveRestartEnable = VK_FALSE;
		return info;
	}

	VkPipelineViewportStateCreateInfo viewport_state_create_info() {
		return {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1,
		};
	}

	VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode, int cullModeBits)
	{
		VkPipelineRasterizationStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.depthClampEnable = VK_FALSE;
		//rasterizer discard allows objects with holes, default to no
		info.rasterizerDiscardEnable = VK_FALSE;

		info.polygonMode = polygonMode;
		info.lineWidth = 1.0f;
		//backface cull = yes
		info.cullMode = cullModeBits;
		info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		//no depth bias
		info.depthBiasEnable = VK_FALSE;
		info.depthBiasConstantFactor = 0.0f;
		info.depthBiasClamp = 0.0f;
		info.depthBiasSlopeFactor = 0.0f;

		return info;
	}
	VkPipelineMultisampleStateCreateInfo multisampling_state_create_info()
	{
		VkPipelineMultisampleStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.sampleShadingEnable = VK_FALSE;
		//multisampling defaulted to no multisampling (1 sample per pixel)
		info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		info.minSampleShading = 1.0f;
		info.pSampleMask = nullptr;
		info.alphaToCoverageEnable = VK_FALSE;
		info.alphaToOneEnable = VK_FALSE;
		return info;
	}
	VkPipelineColorBlendAttachmentState color_blend_attachment_state() {
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		return colorBlendAttachment;
	}
	VkPipelineLayoutCreateInfo pipeline_layout_create_info() {
		VkPipelineLayoutCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.pNext = nullptr;

		//empty defaults
		info.flags = 0;
		info.setLayoutCount = 0;
		info.pSetLayouts = nullptr;
		info.pushConstantRangeCount = 0;
		info.pPushConstantRanges = nullptr;
		return info;
	}

	VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp)
	{
		VkPipelineDepthStencilStateCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		info.pNext = nullptr;

		info.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE;
		info.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE;
		info.depthCompareOp = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
		info.depthBoundsTestEnable = VK_FALSE;
		info.minDepthBounds = 0.0f; // Optional
		info.maxDepthBounds = 1.0f; // Optional
		info.stencilTestEnable = VK_FALSE;

		return info;
	}

	VkImageCreateInfo image_create_info(
		VkFormat format, VkImageUsageFlags usageFlags,
		VkExtent3D extent, uint32_t mipLevels /* = 1*/,
		VkImageCreateFlagBits createFlags/* = (VkImageCreateFlagBits)0*/)
	{
		VkImageCreateInfo info = { };
		info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		info.pNext = nullptr;
		info.flags = createFlags;

		info.imageType = VK_IMAGE_TYPE_2D;

		info.format = format;
		info.extent = extent;

		info.mipLevels = mipLevels;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = usageFlags;

		return info;
	}

	VkImageViewCreateInfo imageview_create_info(
		VkFormat format, VkImage image,
		VkImageAspectFlags aspectFlags, uint32_t baseMipLevel/* = 0*/, VkImageViewType viewType/* = VK_IMAGE_VIEW_TYPE_2D*/)
	{
		//build a image-view for the depth image to use for rendering
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.pNext = nullptr;

		info.viewType = viewType;
		info.image = image;
		info.format = format;
		info.subresourceRange.baseMipLevel = baseMipLevel;
		info.subresourceRange.levelCount = 1;
		info.subresourceRange.baseArrayLayer = 0;
		info.subresourceRange.layerCount = 1;
		info.subresourceRange.aspectMask = aspectFlags;

		return info;
	}

	VkDescriptorSetLayoutBinding descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding)
	{
		VkDescriptorSetLayoutBinding setbind = {};
		setbind.binding = binding;
		setbind.descriptorCount = 1;
		setbind.descriptorType = type;
		setbind.pImmutableSamplers = nullptr;
		setbind.stageFlags = stageFlags;

		return setbind;
	}

	VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding)
	{
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;

		write.dstBinding = binding;
		write.dstSet = dstSet;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.pBufferInfo = bufferInfo;

		return write;
	}

	VkSamplerCreateInfo sampler_create_info(VkFilter filters, VkSamplerAddressMode samplerAddressMode /*= VK_SAMPLER_ADDRESS_MODE_REPEAT*/)
	{
		VkSamplerCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		info.pNext = nullptr;

		info.magFilter = filters;
		info.minFilter = filters;
		info.addressModeU = samplerAddressMode;
		info.addressModeV = samplerAddressMode;
		info.addressModeW = samplerAddressMode;

		return info;
	}

	VkWriteDescriptorSet write_descriptor_image(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorImageInfo* imageInfos, uint32_t binding)
	{
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;

		write.dstBinding = binding;
		write.dstSet = dstSet;
		write.descriptorCount = 1;
		write.descriptorType = type;
		write.pImageInfo = imageInfos;

		return write;
	}

	// Dynamic Rendering
	VkRenderingAttachmentInfo rendering_attachment_info(VkImageView view, VkImageLayout layout)
	{
		constexpr VkClearValue clearValues[2] = {
			{.color = {.float32 = { 0.0f, 0.0f, 0.0f, 0.0f } } },
			{.depthStencil = {.depth = 1.0f, .stencil = 0 } }
		};

		VkClearValue cv;
		if (layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
			cv = clearValues[0];
		} else if (layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
			cv = clearValues[1];
		} else {
			ASSERT(false);
		}

		return {
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
			.imageView = view,
			.imageLayout = layout,
			.resolveMode = VK_RESOLVE_MODE_NONE,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = cv
		};
	}

	VkRenderingInfo rendering_info(
		VkRenderingAttachmentInfo* pColorAttachments, VkRenderingAttachmentInfo* pDepthAttachments, 
		uint32_t width, uint32_t height)
	{
		return {
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
			.renderArea = {
				.extent = { width, height }
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = pColorAttachments,
			.pDepthAttachment = pDepthAttachments
		};
	}
}