#include "stdafx.h"

#include "Enigne.h"
#include "defs.h"

void Engine::Init()
{
    createWindow();
    createInstance();
    createDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    createVmaAllocator();

    createSwapchain();
    createImageViews();
    createRenderPass();

    createGraphicsPipeline();
    createFramebuffers();
    createFrameData();

    loadMeshes();
    createScene();

    _isInitialized = true;
}

void Engine::Run()
{
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        drawFrame();

        _camera.Update(_window, _deltaTime);
        calculateDeltaTime();
    }

    vkDeviceWaitIdle(_device);
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

void Engine::createFrameData()
{
    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = _graphicsQueueFamily
    };

    VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();
    VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& f = _frames[i];

        VKASSERT(vkCreateCommandPool(_device, &poolInfo, nullptr, &f.commandPool));

        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = f.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VKASSERT(vkAllocateCommandBuffers(_device, &allocInfo, &f.mainCmdBuffer));

        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.imageAvailableSemaphore));
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.renderFinishedSemaphore));
        VKASSERT(vkCreateFence(_device, &fenceInfo, nullptr, &f.inFlightFence));

        f.cameraBuffer = createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        _deletionStack.push([&]() { f.cleanup(_device, _allocator); });
    }


    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        
    }

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        _deletionStack.push([&]() {  });
    }
}

void Engine::Cleanup()
{
    if (!_isInitialized) {
        return;
    }
    cleanupSwapchainResources();
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);

    _deletionStack.flush();
}