#include "stdafx.h"
#include "Enigne.h"

void Engine::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = _graphicsQueueFamily
    };

    VKASSERT(vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool));
}

void Engine::createCommandBuffers()
{
    _commandBuffers.resize(_swapchainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = _commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = (uint32_t)_commandBuffers.size()
    };

    VKASSERT(vkAllocateCommandBuffers(_device, &allocInfo, _commandBuffers.data()));
}

void Engine::recordCommandBuffer()
{
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT
    };

    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = _renderPass,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = _swapchainExtent
        },
        .clearValueCount = 1,
        .pClearValues = &clearColor
    };

    for (size_t i = 0; i < _commandBuffers.size(); i++) {
        renderPassInfo.framebuffer = _swapchainFramebuffers[i];

        VKASSERT(vkBeginCommandBuffer(_commandBuffers[i], &beginInfo));

        vkCmdBeginRenderPass(_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);

        vkCmdDraw(_commandBuffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(_commandBuffers[i]);

        VKASSERT(vkEndCommandBuffer(_commandBuffers[i]));
    }
}

