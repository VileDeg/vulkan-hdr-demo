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

    initUploadContext();
    
    createSwapchain();

    createRenderpass();

    prepareMainPass();
    prepareViewportPass(_swapchain.imageExtent.width, _swapchain.imageExtent.height);
    prepareShadowPass();

    initDescriptors();
    

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

        _camera.Update(_window);
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
    
    { // Shadow renderpass
        VkAttachmentDescription osAttachments[2] = {};

        // Find a suitable depth format
        VkBool32 validDepthFormat = utils::getSupportedDepthFormat(_physicalDevice, &_shadow.fbDepthFormat);
        assert(validDepthFormat);

        osAttachments[0].format = ShadowPass::FB_COLOR_FORMAT;
        osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Depth attachment
        osAttachments[1].format = _shadow.fbDepthFormat;
        osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorReference = {};
        colorReference.attachment = 0;
        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthReference = {};
        depthReference.attachment = 1;
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorReference;
        subpass.pDepthStencilAttachment = &depthReference;

        VkRenderPassCreateInfo renderPassCreateInfo = {};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = 2;
        renderPassCreateInfo.pAttachments = osAttachments;
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpass;

        VKASSERT(vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &_shadow.renderpass));
    }

    _deletionStack.push([&]() { 
        vkDestroyRenderPass(_device, _shadow.renderpass, nullptr);
        vkDestroyRenderPass(_device, _viewport.renderpass, nullptr);
        vkDestroyRenderPass(_device, _swapchain.renderpass, nullptr); 
    });
}


void Engine::prepareViewportPass(uint32_t extentX, uint32_t extentY) {
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
    prepareViewportPass(extentX, extentY);

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


void Engine::prepareShadowPass()
{
    _shadow.width = ShadowPass::FB_DIM;
    _shadow.height = ShadowPass::FB_DIM;

    Texture& cubemap = _shadow.cubemapArray;
    // 32 bit float format for higher precision
    VkFormat format = ShadowPass::FB_COLOR_FORMAT;

    const uint32_t cubeArrayLayerCount = 6 * MAX_LIGHTS;

    // Cube map image description
    VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { ShadowPass::TEX_DIM, ShadowPass::TEX_DIM, 1 },
        .mipLevels = 1,
        .arrayLayers = cubeArrayLayerCount,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    //allocate temporary buffer for holding texture data to upload
           //VkDeviceSize imageSize = _shadow.width * _shadow.height;
    VmaAllocationCreateInfo imgAllocinfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    //create cubemap image
    VKASSERT(vmaCreateImage(_allocator, &imageCreateInfo, &imgAllocinfo,
        &cubemap.allocImage.image, &cubemap.allocImage.allocation, nullptr));

    // Image barrier for optimal image (target)
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = cubeArrayLayerCount;

    immediate_submit([&](VkCommandBuffer cmd) {
        utils::setImageLayout(
            cmd,
            cubemap.allocImage.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            subresourceRange);
    });

    // Create image view
    VkImageViewCreateInfo arrayView = {};
    arrayView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    arrayView.viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
    arrayView.format = format;
    arrayView.components = { VK_COMPONENT_SWIZZLE_R };
    arrayView.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    arrayView.subresourceRange.layerCount = cubeArrayLayerCount;
    arrayView.image = cubemap.allocImage.image;
    VKASSERT(vkCreateImageView(_device, &arrayView, nullptr, &cubemap.view));

    // Create sampler
    VkSamplerCreateInfo sampler = {};
    sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler.magFilter = ShadowPass::TEX_FILTER;
    sampler.minFilter = ShadowPass::TEX_FILTER;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler.addressModeV = sampler.addressModeU;
    sampler.addressModeW = sampler.addressModeU;
    sampler.mipLodBias = 0.0f;
    sampler.maxAnisotropy = 1.0f;
    sampler.compareOp = VK_COMPARE_OP_NEVER;
    sampler.minLod = 0.0f;
    sampler.maxLod = 1.0f;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VKASSERT(vkCreateSampler(_device, &sampler, nullptr, &_shadow.sampler));

    for (int i = 0; i < MAX_LIGHTS; ++i) {
        
        Texture& depth = _shadow.depth[i];
        auto& faceViews = _shadow.faceViews[i];
        auto& faceFramebuffers = _shadow.faceFramebuffers[i];

        VkImageViewCreateInfo view = {};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        view.components = { VK_COMPONENT_SWIZZLE_R };
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        view.subresourceRange.layerCount = cubeArrayLayerCount;
        view.image = cubemap.allocImage.image;
        view.subresourceRange.layerCount = 1;
       
        for (uint32_t face = 0; face < 6; ++face)
        {
            view.subresourceRange.baseArrayLayer = i * 6 + face;
            vkCreateImageView(_device, &view, nullptr, &faceViews[face]);
        }

        {
            VkFormat fbColorFormat = ShadowPass::FB_COLOR_FORMAT;

            // Color attachment
            VkImageCreateInfo imageCreateInfo = {};
            imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
            imageCreateInfo.format = fbColorFormat;
            imageCreateInfo.extent.width = _shadow.width;
            imageCreateInfo.extent.height = _shadow.height;
            imageCreateInfo.extent.depth = 1;
            imageCreateInfo.mipLevels = 1;
            imageCreateInfo.arrayLayers = 1;
            imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            // Image of the framebuffer is blit source
            imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            // Depth stencil attachment
            imageCreateInfo.format = _shadow.fbDepthFormat;
            imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

            VmaAllocationCreateInfo imgAllocinfo = {
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                    .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            };

            VKASSERT(vmaCreateImage(_allocator, &imageCreateInfo, &imgAllocinfo,
                &depth.allocImage.image, &depth.allocImage.allocation, nullptr));

            immediate_submit([&](VkCommandBuffer cmd) {
                utils::setImageLayout(
                    cmd,
                    depth.allocImage.image,
                    VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                });

            VkImageViewCreateInfo depthStencilView = {};
            depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
            depthStencilView.format = _shadow.fbDepthFormat;
            depthStencilView.flags = 0;
            depthStencilView.subresourceRange = {};
            depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (_shadow.fbDepthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
                depthStencilView.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            depthStencilView.subresourceRange.baseMipLevel = 0;
            depthStencilView.subresourceRange.levelCount = 1;
            depthStencilView.subresourceRange.baseArrayLayer = 0;
            depthStencilView.subresourceRange.layerCount = 1;
            depthStencilView.image = depth.allocImage.image;
            VKASSERT(vkCreateImageView(_device, &depthStencilView, nullptr, &depth.view));

            VkImageView attachments[2];
            attachments[1] = depth.view;

            VkFramebufferCreateInfo fbufCreateInfo = {};
            fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbufCreateInfo.renderPass = _shadow.renderpass;
            fbufCreateInfo.attachmentCount = 2;
            fbufCreateInfo.pAttachments = attachments;
            fbufCreateInfo.width = _shadow.width;
            fbufCreateInfo.height = _shadow.height;
            fbufCreateInfo.layers = 1;

            for (uint32_t i = 0; i < 6; i++)
            {
                attachments[0] = faceViews[i];
                VKASSERT(vkCreateFramebuffer(_device, &fbufCreateInfo, nullptr, &faceFramebuffers[i]));
            }

        }

        // Cleanup all shadow pass resources for current light
        _deletionStack.push([=]() mutable {
            for (auto& fb : faceFramebuffers) {
                vkDestroyFramebuffer(_device, fb, nullptr);
            }

            // Destroy depth image
            vkDestroyImageView(_device, depth.view, nullptr);
            vmaDestroyImage(_allocator, depth.allocImage.image, depth.allocImage.allocation);

            for (auto& view : faceViews) {
                vkDestroyImageView(_device, view, nullptr);
            }
        });
    }

    _deletionStack.push([=]() mutable {
        // Destroy cubemap image
        vkDestroyImageView(_device, cubemap.view, nullptr);
        vmaDestroyImage(_allocator, cubemap.allocImage.image, cubemap.allocImage.allocation);
        // Destroy sampler
        vkDestroySampler(_device, _shadow.sampler, nullptr);
    });
}




void Engine::initDescriptors()
{
    _descriptorAllocator = new vkutil::DescriptorAllocator{};
    _descriptorAllocator->init(_device);

    _descriptorLayoutCache = new vkutil::DescriptorLayoutCache{};
    _descriptorLayoutCache->init(_device);

    // Dynamic uniform buffer
    {
        const size_t sceneParamBufferSize = MAX_FRAMES_IN_FLIGHT * pad_uniform_buffer_size(sizeof(GPUSceneUB));

        _sceneParameterBuffer =
            createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        _sceneParameterBuffer.descInfo = {
                   .buffer = _sceneParameterBuffer.buffer,
                   .offset = 0,
                   .range = sizeof(GPUSceneUB)
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

    VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily);
    //create pool for upload context
    VKASSERT(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr, &_uploadContext.commandPool));

    //allocate the default command buffer that we will use for the instant commands
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_uploadContext.commandPool, 1);

    VKASSERT(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_uploadContext.commandBuffer));

    _deletionStack.push([&]() {
        vkDestroyCommandPool(_device, _uploadContext.commandPool, nullptr);
        vkDestroyFence(_device, _uploadContext.uploadFence, nullptr);
        });
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
            f.cameraBuffer = createBuffer(sizeof(GPUCameraUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.cameraBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                .bind_buffer(1, &_sceneParameterBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(f.globalSet, _globalSetLayout);
        }

        { // Create descriptor set with scene SSBO, skybox, shadow
            f.objectBuffer = createBuffer(sizeof(GPUSceneSSBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            // Image descriptor for the shadow cube map
            _shadow.cubemapArray.allocImage.descInfo = {
                .sampler = _shadow.sampler,
                .imageView = _shadow.cubemapArray.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.objectBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT) // SSBO
                .bind_image(1, nullptr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Skybox cubemap will be passed later
                .bind_image(2, &_shadow.cubemapArray.allocImage.descInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Shadow cubemap
                .build(f.objectSet, _objectSetLayout);
        }

        { // Shadow pass descriptor set
            f.shadowUB = createBuffer(sizeof(GPUShadowUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.shadowUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                .bind_buffer(1, &f.objectBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // SSBO
                .build(f.shadowPassSet, _shadowSetLayout);
        }

        { // Compute descriptor sets
            f.compSSBO    = createBuffer(sizeof(GPUCompSSBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            f.compSSBO_ro = createBuffer(sizeof(GPUCompSSBO_ReadOnly), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            // Create layout and write descriptor set for COMPUTE histogram step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_buffer(1, &f.compSSBO_ro.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_image(2, nullptr, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image (imageInfo bound later, every frame)
                .build(f.compHistogramSet, _compute.histogram.setLayout);

            // Create layout and write descriptor set for COMPUTE average luminance step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_buffer(1, &f.compSSBO_ro.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .build(f.compAvgLumSet, _compute.averageLuminance.setLayout);

            // Create layout and write descriptor set for COMPUTE tone mapping step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_buffer(1, &f.compSSBO_ro.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_image(2, nullptr, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image (imageInfo bound later, every frame)
                .build(f.compTonemapSet, _compute.toneMapping.setLayout);
        }
    }

    _deletionStack.push([&]() { 
        f.cameraBuffer.destroy(_allocator);
        f.objectBuffer.destroy(_allocator);

        f.shadowUB.destroy(_allocator);

        f.compSSBO.destroy(_allocator);
        f.compSSBO_ro.destroy(_allocator);

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
