#include "stdafx.h"

#include "engine.h"
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

    createRenderpass();

    createSwapchainImages();
    createViewportImages(_swapchain.imageExtent.width, _swapchain.imageExtent.height);

    initDescriptors();
    initUploadContext();

    createFrameData();
    createPipelines();
    createSamplers();
    
    createScene(Engine::modelPath + "sponza/sponza.obj");
    //createScene(Engine::modelPath + "sibenik/sibenik.obj");

    
    initImgui();

    _isInitialized = true;
}

void Engine::Run()
{
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();

        drawFrame();

        _inp.camera.Update(_window);
    }

    vkDeviceWaitIdle(_device);
}

void Engine::Cleanup()
{
    if (!_isInitialized) {
        return;
    }

    cleanupViewportResources();

    cleanupSwapchainResources();
    vkDestroySwapchainKHR(_device, _swapchain.handle, nullptr);

    _deletionStack.flush();
}



static VkRenderPass s_createRenderpass(VkDevice device, VkFormat colorAttFormat, VkImageLayout colorAttFinalLayout, VkFormat depthAttFormat)
{
    VkAttachmentDescription colorAttachment{
        .format = colorAttFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = colorAttFinalLayout
    };

    VkAttachmentDescription depthAttachment{
        .format = depthAttFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass;
    {
        VkAttachmentReference colorAttachmentRef{
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        VkAttachmentReference depthAttachmentRef{
            .attachment = 1,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };

        subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRef,
            .pDepthStencilAttachment = &depthAttachmentRef
        };
    }

    VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkSubpassDependency depthDependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    };

    std::vector<VkSubpassDependency> dependencies = { dependency, depthDependency };
    std::vector<VkAttachmentDescription> attachments = { colorAttachment, depthAttachment };

    VkRenderPassCreateInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = uint32_t(attachments.size()),
        .pAttachments = attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = uint32_t(dependencies.size()),
        .pDependencies = dependencies.data()
    };

    VkRenderPass renderpass;
    VKASSERT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderpass));

    return renderpass;
}

void Engine::createRenderpass() {
    _mainRenderpass = s_createRenderpass(_device, _swapchain.imageFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, _swapchain.depthFormat);
    _viewportRenderpass = s_createRenderpass(_device, _swapchain.imageFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, _swapchain.depthFormat);

    _deletionStack.push([&]() { vkDestroyRenderPass(_device, _mainRenderpass, nullptr); });
    _deletionStack.push([&]() { vkDestroyRenderPass(_device, _viewportRenderpass, nullptr); });
}


void Engine::createViewportImages(uint32_t extentX, uint32_t extentY) {
    _viewport.imageExtent = { extentX, extentY };
    
    VkExtent3D extent3D = {
        extentX,
        extentY,
        1
    };

    //Create depth image
    {
        _viewport.depthFormat = VK_FORMAT_D32_SFLOAT;

        VkImageCreateInfo dimgInfo = vkinit::image_create_info(_viewport.depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, extent3D);

        VmaAllocationCreateInfo dimgAllocinfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        vmaCreateImage(_allocator, &dimgInfo, &dimgAllocinfo, &_viewport.depthImage.image, &_viewport.depthImage.allocation, nullptr);

        VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_viewport.depthFormat, _viewport.depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

        VKASSERT(vkCreateImageView(_device, &dview_info, nullptr, &_viewport.depthImageView));
    }


    VkFormat imageFormat = _swapchain.imageFormat;
    size_t imgCount = _swapchain.images.size();
    _viewport.images.resize(imgCount);

    _viewport.imageViews.resize(imgCount);
    _viewport.framebuffers.resize(imgCount);

    VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_viewportRenderpass, { extentX, extentY });

    for (uint32_t i = 0; i < imgCount; i++)
    {
        VkImageCreateInfo dimg_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = imageFormat,
            .extent = extent3D, // Extent to whole window
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };


        VmaAllocationCreateInfo dimg_allocinfo = {};
        dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        //allocate and create the image
        VKASSERT(vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_viewport.images[i].image, &_viewport.images[i].allocation, nullptr));

        VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(imageFormat, _viewport.images[i].image, VK_IMAGE_ASPECT_COLOR_BIT);

        VKASSERT(vkCreateImageView(_device, &dview_info, nullptr, &_viewport.imageViews[i]));

        std::vector<VkImageView> attachments = {
            _viewport.imageViews[i],
            _viewport.depthImageView
        };

        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();

        VKASSERT(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_viewport.framebuffers[i]));
    }
}

void Engine::recreateViewport(uint32_t extentX, uint32_t extentY)
{
    vkDeviceWaitIdle(_device);

    cleanupViewportResources();
    createViewportImages(extentX, extentY);

    vkDeviceWaitIdle(_device);
}

void Engine::cleanupViewportResources()
{
    for (auto& framebuffer : _viewport.framebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }

    _viewport.framebuffers.clear();

    //Destroy depth image
    vkDestroyImageView(_device, _viewport.depthImageView, nullptr);
    vmaDestroyImage(_allocator, _viewport.depthImage.image, _viewport.depthImage.allocation);

    for (auto& imageView : _viewport.imageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }

    _viewport.imageViews.clear();

    for (auto& image : _viewport.images) {
        vmaDestroyImage(_allocator, image.image, image.allocation);
    }

    _viewport.images.clear();
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

        _sceneParameterBuffer =
            createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        _deletionStack.push([&]() {
            _sceneParameterBuffer.destroy(_allocator);
            });

        VkDescriptorSetLayoutBinding cameraBinding =
            vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

        VkDescriptorSetLayoutBinding sceneBinding =
            vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

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
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            // SSBO, both shaders, binding 0
            vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),
            // Skybox, fragment shader, binding 1
            vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
        };

        VkDescriptorSetLayoutCreateInfo objectSkyboxSetInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .flags = 0,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        };

        VKASSERT(vkCreateDescriptorSetLayout(_device, &objectSkyboxSetInfo, nullptr, &_objectSetLayout));
        _deletionStack.push([&]() {
            vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
            });
    }

    { // Diffuse texture descriptor set
        VkDescriptorSetLayoutBinding textureBind =
            vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

        VkDescriptorSetLayoutCreateInfo diffuseSetInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            // This is a push descriptor set !
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = 1,
            .pBindings = &textureBind
        };

        vkCreateDescriptorSetLayout(_device, &diffuseSetInfo, nullptr, &_diffuseTextureSetLayout);
        _deletionStack.push([&]() {
            vkDestroyDescriptorSetLayout(_device, _diffuseTextureSetLayout, nullptr);
            });
    }
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


void Engine::initFrame(FrameData& f)
{
    {
        // Command pool
        VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = _graphicsQueueFamily
        };

        VKASSERT(vkCreateCommandPool(_device, &poolInfo, nullptr, &f.commandPool));


        // Main command buffer
        VkCommandBufferAllocateInfo cmdBufferAllocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = f.commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VKASSERT(vkAllocateCommandBuffers(_device, &cmdBufferAllocInfo, &f.cmdBuffer));


        // Synchronization primitives
        VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();
        VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.imageAvailableSemaphore));
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.renderFinishedSemaphore));
        VKASSERT(vkCreateFence(_device, &fenceInfo, nullptr, &f.inFlightFence));
    }

    {
        std::vector<VkWriteDescriptorSet> setWrites;
        
        {
            f.cameraBuffer = createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            VkDescriptorSetAllocateInfo camSceneSetAllocInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = _descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &_globalSetLayout
            };
            vkAllocateDescriptorSets(_device, &camSceneSetAllocInfo, &f.globalDescriptor);

            VkDescriptorBufferInfo cameraInfo{
            .buffer = f.cameraBuffer.buffer,
            .offset = 0,
            .range = sizeof(GPUCameraData)
            };

            VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, f.globalDescriptor, &cameraInfo, 0);

            setWrites.push_back(cameraWrite);

            VkDescriptorBufferInfo sceneInfo{
                .buffer = _sceneParameterBuffer.buffer,
                .offset = 0,
                .range = sizeof(GPUSceneData)
            };

            VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, f.globalDescriptor, &sceneInfo, 1);

            setWrites.push_back(sceneWrite);
        }

        {
            // Create SSBO with all objects data
            f.objectBuffer = createBuffer(sizeof(GPUSSBOData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            VkDescriptorSetAllocateInfo objectSetAlloc = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = _descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &_objectSetLayout
            };

            vkAllocateDescriptorSets(_device, &objectSetAlloc, &f.objectDescriptor);

            VkDescriptorBufferInfo objectInfo = {
                .buffer = f.objectBuffer.buffer,
                .offset = 0,
                .range = sizeof(GPUSSBOData)
            };

            

            VkWriteDescriptorSet objectWrite = vkinit::write_descriptor_buffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, f.objectDescriptor, &objectInfo, 0);

            setWrites.push_back(objectWrite);
            //setWrites.push_back(skyboxWrite);
        }

        vkUpdateDescriptorSets(_device, setWrites.size(), setWrites.data(), 0, nullptr);
    }

    _deletionStack.push([&]() { 
        f.cameraBuffer.destroy(_allocator);
        f.objectBuffer.destroy(_allocator);

        vkDestroyFence(_device, f.inFlightFence, nullptr);
        vkDestroySemaphore(_device, f.imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(_device, f.renderFinishedSemaphore, nullptr);

        vkDestroyCommandPool(_device, f.commandPool, nullptr);
    });
}

void Engine::createFrameData()
{
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        initFrame(_frames[i]);
    }
}
