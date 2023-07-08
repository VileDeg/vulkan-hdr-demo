#include "stdafx.h"
#include "engine.h"

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

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

    VmaAllocationCreateInfo allocInfo = {
        .usage = memoryUsage
    };

    AllocatedBuffer newBuffer;
    newBuffer.create(_allocator, bufferInfo, allocInfo);

    newBuffer.descInfo = {
        .buffer = newBuffer.buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };

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

void AllocatedBuffer::create(VmaAllocator allocator, VkBufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo) {
    VKASSERT(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
        &buffer, &allocation, nullptr));

    if (allocInfo.usage == VMA_MEMORY_USAGE_CPU_ONLY ||
        allocInfo.usage == VMA_MEMORY_USAGE_CPU_TO_GPU ||
        allocInfo.usage == VMA_MEMORY_USAGE_GPU_TO_CPU)
    {
        hostVisible = true;
        VKASSERT(vmaMapMemory(allocator, allocation, &gpu_ptr));
    } else {
        hostVisible = false;
    }
}

void AllocatedBuffer::destroy(const VmaAllocator& allocator) {
    if (hostVisible) {
        vmaUnmapMemory(allocator, allocation);
    }
    vmaDestroyBuffer(allocator, buffer, allocation);
}