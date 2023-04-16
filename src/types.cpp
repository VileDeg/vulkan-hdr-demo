#include "stdafx.h"
#include "types.h"

void FrameData::cleanup(const VkDevice& device, const VmaAllocator& allocator) {
    objectBuffer.destroy(allocator);
    
    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
    vkDestroyFence(device, inFlightFence, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);

    cameraBuffer.destroy(allocator);
}