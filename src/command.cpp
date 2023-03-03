#include "stdafx.h"
#include "Enigne.h"


void Engine::createCommandBuffers()
{
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = _graphicsQueueFamily
    };

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VKASSERT(vkCreateCommandPool(_device, &poolInfo, nullptr, &_frames[i].commandPool));

        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = _frames[i].commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VKASSERT(vkAllocateCommandBuffers(_device, &allocInfo, &_frames[i].mainCmdBuffer));

    }
}

