#include "stdafx.h"

#include "engine.h"



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


    _descriptorAllocator->cleanup();
    _descriptorLayoutCache->cleanup();

    vkDestroySwapchainKHR(_device, _swapchainHandle, nullptr);

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
    _swapchain.renderpass = s_createRenderpass(_device, _swapchain.imageFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, _swapchain.depthFormat);
    //VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    _viewport.renderpass = s_createRenderpass(_device, _viewport.imageFormat, VK_IMAGE_LAYOUT_GENERAL, _viewport.depthFormat);

    _deletionStack.push([&]() { vkDestroyRenderPass(_device, _swapchain.renderpass, nullptr); });
    _deletionStack.push([&]() { vkDestroyRenderPass(_device, _viewport.renderpass, nullptr); });
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
        VkImageCreateInfo dimgInfo = vkinit::image_create_info(_viewport.depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, extent3D);

        VmaAllocationCreateInfo dimgAllocinfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        vmaCreateImage(_allocator, &dimgInfo, &dimgAllocinfo, &_viewport.depthImage.image, &_viewport.depthImage.allocation, nullptr);

        VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_viewport.depthFormat, _viewport.depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

        VKASSERT(vkCreateImageView(_device, &dview_info, nullptr, &_viewport.depthImageView));
    }


    // 32-bit float HDR format
    size_t imgCount = _swapchain.images.size();
    _viewportImages.resize(imgCount);

    _viewport.imageViews.resize(imgCount);
    _viewport.framebuffers.resize(imgCount);

    VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_viewport.renderpass, { extentX, extentY });

    for (uint32_t i = 0; i < imgCount; i++)
    {
        VkImageCreateInfo dimg_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = _viewport.imageFormat,
            .extent = extent3D, // Extent to whole window
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };


        VmaAllocationCreateInfo dimg_allocinfo = {};
        dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        //allocate and create the image
        VKASSERT(vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_viewportImages[i].image, &_viewportImages[i].allocation, nullptr));

        VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_viewport.imageFormat, _viewportImages[i].image, VK_IMAGE_ASPECT_COLOR_BIT);

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

    for (auto& image : _viewportImages) {
        vmaDestroyImage(_allocator, image.image, image.allocation);
    }

    _viewportImages.clear();
}


void Engine::initDescriptors()
{
    _descriptorAllocator = new vkutil::DescriptorAllocator{};
    _descriptorAllocator->init(_device);

    _descriptorLayoutCache = new vkutil::DescriptorLayoutCache{};
    _descriptorLayoutCache->init(_device);

    // Dynamic unigform buffer
    {
        const size_t sceneParamBufferSize = MAX_FRAMES_IN_FLIGHT * pad_uniform_buffer_size(sizeof(GPUSceneData));

        _sceneParameterBuffer =
            createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        _sceneParameterBuffer.descInfo = {
                   .buffer = _sceneParameterBuffer.buffer,
                   .offset = 0,
                   .range = sizeof(GPUSceneData)
        };
        _deletionStack.push([&]() {
            _sceneParameterBuffer.destroy(_allocator);
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

        /*VKASSERT(vkCreateDescriptorSetLayout(_device, &objectSkyboxSetInfo, nullptr, &_objectSetLayout));
        _deletionStack.push([&]() {
            vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
            });*/

        _objectSetLayout = _descriptorLayoutCache->create_descriptor_layout(&objectSkyboxSetInfo);
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

     
        _diffuseTextureSetLayout = _descriptorLayoutCache->create_descriptor_layout(&diffuseSetInfo);
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

        VKASSERT(vkAllocateCommandBuffers(_device, &cmdBufferAllocInfo, &f.cmd));


        // Synchronization primitives
        VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();
        VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.imageAvailableSemaphore));
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.renderFinishedSemaphore));

        VKASSERT(vkCreateFence(_device, &fenceInfo, nullptr, &f.inFlightFence));
    }

    {
        { // Create uniform buffer with camera data
            f.cameraBuffer = createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            f.cameraBuffer.descInfo = {
                .buffer = f.cameraBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            };
        
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.cameraBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                .bind_buffer(1, &_sceneParameterBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(f.globalSet, _globalSetLayout);
        }

        { // Create SSBO with all objects data
            f.objectBuffer = createBuffer(sizeof(GPUSSBOData), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            f.objectBuffer.descInfo = {
                .buffer = f.objectBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            };

            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.objectBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT) // SSBO
                .bind_image(1, nullptr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Skybox cubemap will be passed later
                .build(f.objectSet, _objectSetLayout);
        }

        {
            f.compLumBuffer = createBuffer(sizeof(GPUCompSSBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            f.compLumBuffer.descInfo = {
                .buffer = f.compLumBuffer.buffer,
                .offset = 0,
                .range = VK_WHOLE_SIZE
            };

            // Create layout and write descriptor set for COMPUTE histogram step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compLumBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_image(1, nullptr, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image (imageInfo bound later, every frame)
                .build(f.compHistogramSet, _compute.histogram.setLayout);

            // Create layout and write descriptor set for COMPUTE average luminance step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compLumBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .build(f.compAvgLumSet, _compute.averageLuminance.setLayout);

            // Create layout and write descriptor set for COMPUTE tone mapping step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compLumBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_image(1, nullptr, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image (imageInfo bound later, every frame)
                .build(f.compTonemapSet, _compute.toneMapping.setLayout);
        }
    }

    _deletionStack.push([&]() { 
        f.cameraBuffer.destroy(_allocator);
        f.objectBuffer.destroy(_allocator);

        f.compLumBuffer.destroy(_allocator);

        vkDestroyFence(_device, f.inFlightFence, nullptr);

        vkDestroySemaphore(_device, f.imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(_device, f.renderFinishedSemaphore, nullptr);
        //vkDestroySemaphore(_device, f.graphicsToComputeSemaphore, nullptr);

        vkDestroyCommandPool(_device, f.commandPool, nullptr);
    });
}

void Engine::createFrameData()
{
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        initFrame(_frames[i]);
    }
}
