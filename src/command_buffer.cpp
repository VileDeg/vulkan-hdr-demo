#include "stdafx.h"
#include "Enigne.h"

void Engine::createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VKASSERT(vkCreateCommandPool(_device, &poolInfo, nullptr, &_commandPool));
}

void Engine::createCommandBuffers()
{
    _commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = _commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = (uint32_t)_commandBuffers.size()
    };

    VKASSERT(vkAllocateCommandBuffers(_device, &allocInfo, _commandBuffers.data()));
}



