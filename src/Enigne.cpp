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

    createFrameData();
    createPipeline();
    createFramebuffers();

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


size_t Engine::pad_uniform_buffer_size(size_t originalSize)
{
    //From https://github.com/SaschaWillems/Vulkan/tree/master/examples/dynamicuniformbuffer
	// Calculate required alignment based on minimum device offset alignment
    size_t minUboAlignment = _gpuProperties.limits.minUniformBufferOffsetAlignment;
	size_t alignedSize = originalSize;
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
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
    

    uint32_t poolCount = 10;
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, poolCount },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, poolCount }
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = poolCount,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VKASSERT(vkCreateDescriptorPool(_device, &descriptorPoolInfo, nullptr, &_descriptorPool));
    _deletionStack.push([&]() {
        vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    });

    const size_t sceneParamBufferSize = MAX_FRAMES_IN_FLIGHT * pad_uniform_buffer_size(sizeof(GPUSceneData));

    _sceneParameterBuffer = createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    _deletionStack.push([&]() {
        vmaDestroyBuffer(_allocator, _sceneParameterBuffer.buffer, _sceneParameterBuffer.allocation);
    });

    //binding for camera data at 0
    VkDescriptorSetLayoutBinding cameraBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

    //binding for scene data at 1
    VkDescriptorSetLayoutBinding sceneBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

    VkDescriptorSetLayoutBinding bindings[] = { cameraBinding, sceneBinding };

    VkDescriptorSetLayoutCreateInfo cameraBufferLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };

    VKASSERT(vkCreateDescriptorSetLayout(_device, &cameraBufferLayoutInfo, nullptr, &_globalSetLayout));
    _deletionStack.push([&]() { vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr); });

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        auto& f = _frames[i];

        VKASSERT(vkCreateCommandPool(_device, &poolInfo, nullptr, &f.commandPool));

        VkCommandBufferAllocateInfo cmdBufferAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = f.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VKASSERT(vkAllocateCommandBuffers(_device, &cmdBufferAllocInfo, &f.mainCmdBuffer));

        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.imageAvailableSemaphore));
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.renderFinishedSemaphore));
        VKASSERT(vkCreateFence(_device, &fenceInfo, nullptr, &f.inFlightFence));

        _deletionStack.push([&]() { f.cleanup(_device, _allocator); });

        f.cameraBuffer = createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        VkDescriptorSetAllocateInfo descSetAllocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = _descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &_globalSetLayout
        };
        vkAllocateDescriptorSets(_device, &descSetAllocInfo, &f.globalDescriptor);

        VkDescriptorBufferInfo cameraInfo{
            .buffer = f.cameraBuffer.buffer,
            .offset = 0,
            .range = sizeof(GPUCameraData)
        };

        //pad_uniform_buffer_size(sizeof(GPUSceneData)) * i
        VkDescriptorBufferInfo sceneInfo{
            .buffer = _sceneParameterBuffer.buffer,
            .offset = 0,
            .range = sizeof(GPUSceneData)
        };

        VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _frames[i].globalDescriptor,&cameraInfo,0);

		VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, _frames[i].globalDescriptor, &sceneInfo, 1);

		VkWriteDescriptorSet setWrites[] = { cameraWrite,sceneWrite };

        vkUpdateDescriptorSets(_device, 2, setWrites, 0, nullptr);
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