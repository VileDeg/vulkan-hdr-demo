#include "stdafx.h"
#include "Engine.h"

void Engine::createVmaAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo = {
        .physicalDevice = _physicalDevice,
        .device = _device,
        .instance = _instance,
    };
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _deletionStack.push([&]() { vmaDestroyAllocator(_allocator); });
}

AllocatedBuffer Engine::createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = allocSize,
        .usage = usage
    };

    VmaAllocationCreateInfo vmaallocInfo = {
        .usage = memoryUsage
    };

    AllocatedBuffer newBuffer;
    VKASSERT(vmaCreateBuffer(_allocator, &bufferInfo, &vmaallocInfo,
        &newBuffer.buffer, &newBuffer.allocation, nullptr));

    return newBuffer;
}

size_t Engine::pad_uniform_buffer_size(size_t originalSize)
{
    // From https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
    // Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = _gpuProperties.limits.minUniformBufferOffsetAlignment;
    size_t alignedSize = originalSize;
    if (minUboAlignment > 0) {
        alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
    }
    return alignedSize;
}



void Engine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VkCommandBuffer cmd = _uploadContext.commandBuffer;

    //begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VKASSERT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    //execute the function
    function(cmd);

    VKASSERT(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = vkinit::submit_info(&cmd);

    //submit command buffer to the queue and execute it.
    // _uploadFence will now block until the graphic commands finish execution
    VKASSERT(vkQueueSubmit(_graphicsQueue, 1, &submit, _uploadContext.uploadFence));

    vkWaitForFences(_device, 1, &_uploadContext.uploadFence, true, 9999999999);
    vkResetFences(_device, 1, &_uploadContext.uploadFence);

    // reset the command buffers inside the command pool
    vkResetCommandPool(_device, _uploadContext.commandPool, 0);
}
