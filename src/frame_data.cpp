#include "stdafx.h"
#include "Engine.h"

void Engine::initFrame(FrameData& f)
{
    {
        VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = _graphicsQueueFamily
        };

        VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();
        VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

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
    }

    {
        f.cameraBuffer = createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        VkDescriptorSetAllocateInfo camSceneSetAllocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = _descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &_globalSetLayout
        };
        vkAllocateDescriptorSets(_device, &camSceneSetAllocInfo, &f.globalDescriptor);

        // Create SSBO with all objects data
        const int MAX_OBJECTS = 10000; 
        const int SSBO_SIZE = sizeof(glm::uvec4) + sizeof(GPUObjectData) * MAX_OBJECTS;
        //const int SSBO_SIZE = sizeof(GPUObjectData) * MAX_OBJECTS;
        f.objectBuffer = createBuffer(SSBO_SIZE, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        VkDescriptorSetAllocateInfo objectSetAlloc = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = _descriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &_objectSetLayout
        };

        vkAllocateDescriptorSets(_device, &objectSetAlloc, &f.objectDescriptor);

        VkDescriptorBufferInfo cameraInfo{
            .buffer = f.cameraBuffer.buffer,
            .offset = 0,
            .range = sizeof(GPUCameraData)
        };

        VkDescriptorBufferInfo sceneInfo{
            .buffer = _sceneParameterBuffer.buffer,
            .offset = 0,
            .range = sizeof(GPUSceneData)
        };

        VkDescriptorBufferInfo objectInfo{
            .buffer = f.objectBuffer.buffer,
            .offset = 0,
            .range = SSBO_SIZE
        };

        VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, f.globalDescriptor, &cameraInfo, 0);
        VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, f.globalDescriptor, &sceneInfo, 1);
        VkWriteDescriptorSet objectWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, f.objectDescriptor, &objectInfo, 0);

        VkWriteDescriptorSet setWrites[] = { cameraWrite, sceneWrite, objectWrite };

        vkUpdateDescriptorSets(_device, 3, setWrites, 0, nullptr);
    }

    _deletionStack.push([&]() { f.cleanup(_device, _allocator); });
}

void Engine::initDescriptors()
{
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 }, // For camera data
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 }, // For scene data
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 }, // For object data SSBO
        //add combined-image-sampler descriptor types to the pool
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 } // For textures
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 10,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VKASSERT(vkCreateDescriptorPool(_device, &descriptorPoolInfo, nullptr, &_descriptorPool));
    _deletionStack.push([&]() {
        vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    });


    { // Camera + scene descriptor set
        const size_t sceneParamBufferSize = MAX_FRAMES_IN_FLIGHT * pad_uniform_buffer_size(sizeof(GPUSceneData));

        _sceneParameterBuffer = createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        _deletionStack.push([&]() {
            _sceneParameterBuffer.destroy(_allocator);
        });

        VkDescriptorSetLayoutBinding cameraBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

        VkDescriptorSetLayoutBinding sceneBinding = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

        VkDescriptorSetLayoutBinding bindings[] = { cameraBinding, sceneBinding };

        VkDescriptorSetLayoutCreateInfo cameraBufferLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 2,
            .pBindings = bindings
        };

        VKASSERT(vkCreateDescriptorSetLayout(_device, &cameraBufferLayoutInfo, nullptr, &_globalSetLayout));
        _deletionStack.push([&]() {
            vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);
        });
    }

    { // Object descriptor set (SSBO)
        VkDescriptorSetLayoutBinding objectBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0);

        VkDescriptorSetLayoutCreateInfo set2info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = 0,
            .bindingCount = 1,
            .pBindings = &objectBind
        };

        VKASSERT(vkCreateDescriptorSetLayout(_device, &set2info, nullptr, &_objectSetLayout));
        _deletionStack.push([&]() {
            vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
        });
    }

    //another set, one that holds a single texture
    VkDescriptorSetLayoutBinding textureBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

    VkDescriptorSetLayoutCreateInfo set3info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &textureBind
    };

    vkCreateDescriptorSetLayout(_device, &set3info, nullptr, &_singleTextureSetLayout);
    _deletionStack.push([&]() {
        vkDestroyDescriptorSetLayout(_device, _singleTextureSetLayout, nullptr);
    });
}

void Engine::initUploadContext()
{
    VkFenceCreateInfo uploadFenceCreateInfo = vkinit::fence_create_info();
    VKASSERT(vkCreateFence(_device, &uploadFenceCreateInfo, nullptr, &_uploadContext.uploadFence));
    _deletionStack.push([&]() {
        vkDestroyFence(_device, _uploadContext.uploadFence, nullptr);
        });

    VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily);
    //create pool for upload context
    VKASSERT(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr, &_uploadContext.commandPool));

    _deletionStack.push([=]() {
        vkDestroyCommandPool(_device, _uploadContext.commandPool, nullptr);
        });

    //allocate the default command buffer that we will use for the instant commands
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_uploadContext.commandPool, 1);

    VKASSERT(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_uploadContext.commandBuffer));
}

void Engine::createFrameData()
{
    initDescriptors();
    initUploadContext();

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        initFrame(_frames[i]);
    }
}
