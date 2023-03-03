#include "stdafx.h"
#include "Enigne.h"

void Engine::createSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();
    VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_frames[i].imageAvailableSemaphore));
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_frames[i].renderFinishedSemaphore));
        VKASSERT(vkCreateFence(_device, &fenceInfo, nullptr, &_frames[i].inFlightFence));
    }
}

