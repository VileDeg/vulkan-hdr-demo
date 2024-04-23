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

AllocatedBuffer Engine::allocateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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
    /*
    * Vulkan Example - Dynamic uniform buffers
    *
    * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
    *
    * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
    *
    * Summary:
    * Demonstrates the use of dynamic uniform buffers.
    *
    * Instead of using one uniform buffer per-object, this example allocates one big uniform buffer
    * with respect to the alignment reported by the device via minUniformBufferOffsetAlignment that
    * contains all matrices for the objects in the scene.
    *
    * The used descriptor type VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC then allows to set a dynamic
    * offset used to pass data from the single uniform buffer to the connected shader binding point.
    */

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
    // Function is based on https://github.com/vblanco20-1/vulkan-guide
    /*The MIT License (MIT)

    Copyright (c) 2016 Patrick Marsceill

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.*/

    VkCommandBuffer cmd = _uploadContext.commandBuffer;

    // Begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_ASSERT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // Execute the function
    function(cmd);

    VK_ASSERT(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit = vkinit::submit_info(&cmd);

    // Submit command buffer to the queue and execute it.
    // uploadFence will now block until the graphic commands finish execution
    VK_ASSERT(vkQueueSubmit(_graphicsQueue, 1, &submit, _uploadContext.uploadFence));

    vkWaitForFences(_device, 1, &_uploadContext.uploadFence, true, 9999999999);
    vkResetFences(_device, 1, &_uploadContext.uploadFence);

    // Reset the command buffers inside the command pool
    vkResetCommandPool(_device, _uploadContext.commandPool, 0);
}

void AllocatedBuffer::create(VmaAllocator allocator, VkBufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo) {
    VK_ASSERT(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
        &buffer, &allocation, nullptr));

    if (allocInfo.usage == VMA_MEMORY_USAGE_CPU_ONLY   ||
        allocInfo.usage == VMA_MEMORY_USAGE_CPU_TO_GPU ||
        allocInfo.usage == VMA_MEMORY_USAGE_GPU_TO_CPU)
    {
        hostVisible = true;
        VK_ASSERT(vmaMapMemory(allocator, allocation, &gpu_ptr));
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