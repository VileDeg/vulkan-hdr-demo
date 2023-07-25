#pragma once

namespace utils {
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

	void setDebugName(VkDevice device, VkObjectType type, void* handle, const std::string name);
}