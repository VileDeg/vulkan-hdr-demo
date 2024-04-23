#pragma once

/* Based on https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanTools.cpp */
/*
* Assorted commonly used Vulkan helper functions
*
* Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

/* Initial code was modified */

namespace vk_utils {
	// Create an image memory barrier for changing the layout of
	// an image and put it into an active command buffer
	void setImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkImageSubresourceRange subresourceRange,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);


	// Fixed sub resource on first mip level and layer
	void setImageLayout(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkImageAspectFlags aspectMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);


	void imageMemoryBarrier(
		VkCommandBuffer         command_buffer,
		VkImage                 image,
		VkAccessFlags           src_access_mask,
		VkAccessFlags           dst_access_mask,
		VkImageLayout           old_layout,
		VkImageLayout           new_layout,
		VkPipelineStageFlags    src_stage_mask,
		VkPipelineStageFlags    dst_stage_mask,
		VkImageSubresourceRange subresource_range);

	void memoryBarrier(
		VkCommandBuffer         command_buffer,
		VkAccessFlags           src_access_mask,
		VkAccessFlags           dst_access_mask,
		VkPipelineStageFlags    src_stage_mask,
		VkPipelineStageFlags    dst_stage_mask);

	VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat* depthFormat);
	VkBool32 getSupportedDepthStencilFormat(VkPhysicalDevice physicalDevice, VkFormat* depthStencilFormat);

	VkBool32 formatHasStencil(VkFormat format);

	std::vector<char> readShaderBinary(const std::string& filename);
	bool createShaderModule(VkDevice device, const std::vector<char>& code, VkShaderModule* module);
}