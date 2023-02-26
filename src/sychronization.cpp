#include "Enigne.h"
#include "vk_initializers.h"

void Engine::createSyncObjects()
{
    _imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    _inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();

    VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]));
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]));
        VKASSERT(vkCreateFence(_device, &fenceInfo, nullptr, &_inFlightFences[i]));
    }
}


