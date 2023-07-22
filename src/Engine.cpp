#include "stdafx.h"
#include "engine.h"

void Engine::Init()
{
    createWindow();
    createInstance();
    //createDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    createVmaAllocator();

    initUploadContext();
    
    createSwapchain();

    //createRenderpass();

    prepareMainPass();
    prepareViewportPass(_swapchain.imageExtent.width, _swapchain.imageExtent.height);
    prepareShadowPass();

    initDescriptors();

    createFrameData();
    createPipelines();
    createSamplers();
    
    loadScene(scenePath + "dobrovic-sponza.json");
    //loadScene(scenePath + "crytek-sponza.json");
    
    initImgui();

    _isInitialized = true;
}

void Engine::Run()
{
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();

        drawFrame();

        // Block camera movement when cursor is ON and viewport not hovered
        if (_isViewportHovered || !_cursorEnabled) {
            _camera.Update(_window, _deltaTime);
        }
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



static VkRenderPass s_createRenderpass(VkDevice device, VkFormat colorAttFormat, VkImageLayout colorAttFinalLayout, VkFormat depthAttFormat, const std::vector<VkSubpassDependency>& dependencies)
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

//void Engine::createRenderpass() {
//    std::vector<VkSubpassDependency> dependencies = {
//
//        { // Dependency for upcoming compute shader
//            .srcSubpass = 0,
//            .dstSubpass = VK_SUBPASS_EXTERNAL,
//            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//            .dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
//            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
//        },
//        //{ // Dependency for presenting frame to screen
//        //    .srcSubpass = VK_SUBPASS_EXTERNAL,
//        //    .dstSubpass = 0,
//        //    .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//        //    .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//        //    .srcAccessMask = 0,
//        //    .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
//        //},
//        // Depth attachment dependency
//        /*{ 
//            .srcSubpass = 0,
//            .dstSubpass = VK_SUBPASS_EXTERNAL,
//            .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
//            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
//            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
//            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
//        },
//        {
//            .srcSubpass = VK_SUBPASS_EXTERNAL,
//            .dstSubpass = 0,
//            .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
//            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
//            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
//            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
//        }*/
//
//    };
//
//    _viewport.renderpass = s_createRenderpass(_device, _viewport.imageFormat, VK_IMAGE_LAYOUT_GENERAL, _viewport.depthFormat, dependencies);
//    
//    
//    std::vector<VkSubpassDependency> dependencies1 = {
//        // Color attachment dependency
//        { // Dependency for presenting frame to screen
//            .srcSubpass = VK_SUBPASS_EXTERNAL,
//            .dstSubpass = 0,
//            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//            .srcAccessMask = 0,
//            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
//        },
//     
//        // Depth attachment dependency
//        { // Dependency for presenting frame to screen
//            .srcSubpass = VK_SUBPASS_EXTERNAL,
//            .dstSubpass = 0,
//            .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
//            .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
//            .srcAccessMask = 0,
//            .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
//        }
//    };
//
//    _swapchain.renderpass = s_createRenderpass(_device, _swapchain.imageFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, _swapchain.depthFormat, dependencies1);
// 
//    
//    { // Shadow renderpass
//        VkAttachmentDescription osAttachments[2] = {};
//
//        // Find a suitable depth format
//        VkBool32 validDepthFormat = utils::getSupportedDepthFormat(_physicalDevice, &_shadow.fbDepthFormat);
//        assert(validDepthFormat);
//
//        osAttachments[0].format = ShadowPass::FB_COLOR_FORMAT;
//        osAttachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
//        osAttachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
//        osAttachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
//        osAttachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//        osAttachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//        osAttachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//        osAttachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//
//        // Depth attachment
//        osAttachments[1].format = _shadow.fbDepthFormat;
//        osAttachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
//        osAttachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
//        osAttachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
//        osAttachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
//        osAttachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
//        osAttachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
//        osAttachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
//
//        VkAttachmentReference colorReference = {};
//        colorReference.attachment = 0;
//        colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
//
//        VkAttachmentReference depthReference = {};
//        depthReference.attachment = 1;
//        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
//
//
//        VkSubpassDescription subpass = {};
//        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
//        subpass.colorAttachmentCount = 1;
//        subpass.pColorAttachments = &colorReference;
//        subpass.pDepthStencilAttachment = &depthReference;
//
//        VkSubpassDependency dependency{
//            .srcSubpass = 0,
//            .dstSubpass = VK_SUBPASS_EXTERNAL,
//            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
//            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
//            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
//        };
//
//
//        std::vector<VkSubpassDependency> dependencies = { dependency/*, depthDependency*/ };
//
//        
//
//        VkRenderPassCreateInfo renderPassCreateInfo = {};
//        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
//        renderPassCreateInfo.attachmentCount = 2;
//        renderPassCreateInfo.pAttachments = osAttachments;
//        renderPassCreateInfo.subpassCount = 1;
//        renderPassCreateInfo.pSubpasses = &subpass;
//        renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(dependencies.size()),
//        renderPassCreateInfo.pDependencies = dependencies.data();
//
//
//        VKASSERT(vkCreateRenderPass(_device, &renderPassCreateInfo, nullptr, &_shadow.renderpass));
//    }
//
//    setDebugName(VK_OBJECT_TYPE_RENDER_PASS, _swapchain.renderpass, "Swapchain Renderpass");
//    setDebugName(VK_OBJECT_TYPE_RENDER_PASS, _viewport.renderpass, "Viewport Renderpass");
//    setDebugName(VK_OBJECT_TYPE_RENDER_PASS, _shadow.renderpass, "Shadow Renderpass");
//
//    _deletionStack.push([&]() { 
//        vkDestroyRenderPass(_device, _shadow.renderpass, nullptr);
//        vkDestroyRenderPass(_device, _viewport.renderpass, nullptr);
//        vkDestroyRenderPass(_device, _swapchain.renderpass, nullptr); 
//    });
//}


void Engine::prepareViewportPass(uint32_t extentX, uint32_t extentY) {
    _viewport.imageExtent = { extentX, extentY };
    bool anyFormats = utils::getSupportedDepthFormat(_physicalDevice, &_viewport.depthFormat);
    ASSERTMSG(anyFormats, "Physical device has no supported depth formats");
    
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

        setDebugName(VK_OBJECT_TYPE_IMAGE, _viewport.depthImage.image, "Viewport Depth Image");
    }


    // 32-bit float HDR format
    size_t imgCount = _swapchainImages.size();
    _viewportImages.resize(imgCount);

    _viewport.imageViews.resize(imgCount);
    //_viewport.framebuffers.resize(imgCount);

    //VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_viewport.renderpass, { extentX, extentY });

    for (uint32_t i = 0; i < imgCount; i++)
    {
        VkImageCreateInfo dimg_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = _viewport.colorFormat,
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
        setDebugName(VK_OBJECT_TYPE_IMAGE, _viewportImages[i].image, "Viewport Image " + std::to_string(i));

        VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_viewport.colorFormat, _viewportImages[i].image, VK_IMAGE_ASPECT_COLOR_BIT);

        VKASSERT(vkCreateImageView(_device, &dview_info, nullptr, &_viewport.imageViews[i]));
        setDebugName(VK_OBJECT_TYPE_IMAGE, _viewport.imageViews[i], "Viewport Image View " + std::to_string(i));

        /*std::vector<VkImageView> attachments = {
            _viewport.imageViews[i],
            _viewport.depthImageView
        };*/

        //framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        //framebufferInfo.pAttachments = attachments.data();

        //VKASSERT(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_viewport.framebuffers[i]));
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
    /*for (auto& framebuffer : _viewport.framebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }*/

    //_viewport.framebuffers.clear();

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
    _shadow.width = ShadowPass::TEX_DIM;
    _shadow.height = ShadowPass::TEX_DIM;

    VkBool32 validDepthFormat = utils::getSupportedDepthFormat(_physicalDevice, &_shadow.depthFormat);
    ASSERTMSG(validDepthFormat, "Physical device has no supported depth formats");

    Texture& cubemap = _shadow.cubemapArray;
    // 32 bit float format for higher precision
    const uint32_t cubeArrayLayerCount = 6 * MAX_LIGHTS;

    // Cube map image description
    VkImageCreateInfo imageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = _shadow.colorFormat,
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
    arrayView.format = _shadow.colorFormat;
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


    auto& depth = _shadow.depth;
    { // Depth framebuffer attachment
        
        // Depth attachment image
        VkImageCreateInfo imageCreateInfo = {};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width = _shadow.width;
        imageCreateInfo.extent.height = _shadow.height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        // Image of the framebuffer is blit source
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        imageCreateInfo.format = _shadow.depthFormat;
        imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo imgAllocinfo = {
                .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        VKASSERT(vmaCreateImage(_allocator, &imageCreateInfo, &imgAllocinfo,
            &depth.allocImage.image, &depth.allocImage.allocation, nullptr));

        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (utils::formatHasStencil(_shadow.depthFormat)) {
            aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        immediate_submit([&](VkCommandBuffer cmd) {
            utils::setImageLayout(
                cmd,
                depth.allocImage.image,
                aspectMask,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        });

        VkImageViewCreateInfo depthStencilView = {};
        depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthStencilView.format = _shadow.depthFormat;
        depthStencilView.flags = 0;
        depthStencilView.subresourceRange = {};
        depthStencilView.subresourceRange.aspectMask = aspectMask;
        depthStencilView.subresourceRange.baseMipLevel = 0;
        depthStencilView.subresourceRange.levelCount = 1;
        depthStencilView.subresourceRange.baseArrayLayer = 0;
        depthStencilView.subresourceRange.layerCount = 1;
        depthStencilView.image = depth.allocImage.image;
        VKASSERT(vkCreateImageView(_device, &depthStencilView, nullptr, &depth.view));
    }

    VkImageView attachments[2];
    attachments[1] = depth.view;

    /*VkFramebufferCreateInfo fbufCreateInfo = {};
    fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbufCreateInfo.renderPass = _shadow.renderpass;
    fbufCreateInfo.attachmentCount = 2;
    fbufCreateInfo.pAttachments = attachments;
    fbufCreateInfo.width = _shadow.width;
    fbufCreateInfo.height = _shadow.height;
    fbufCreateInfo.layers = 1;*/

    for (int i = 0; i < MAX_LIGHTS; ++i) {
        
        //Texture& depth = _shadow.depth[i];
        auto& faceViews = _shadow.faceViews[i];
        //auto& faceFramebuffers = _shadow.faceFramebuffers[i];

        VkImageViewCreateInfo view = {};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = _shadow.colorFormat;
        view.components = { VK_COMPONENT_SWIZZLE_R };
        view.image = cubemap.allocImage.image;
        view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        view.subresourceRange.layerCount = 1;
       
        for (uint32_t face = 0; face < 6; ++face)
        {
            view.subresourceRange.baseArrayLayer = i * 6 + face;
            VKASSERT(vkCreateImageView(_device, &view, nullptr, &faceViews[face]));

            //attachments[0] = faceViews[face];
            //VKASSERT(vkCreateFramebuffer(_device, &fbufCreateInfo, nullptr, &faceFramebuffers[face]));
        }

        // Cleanup all shadow pass resources for current light
        _deletionStack.push([=]() mutable {
            /*for (auto& fb : faceFramebuffers) {
                vkDestroyFramebuffer(_device, fb, nullptr);
            }*/

            for (auto& view : faceViews) {
                vkDestroyImageView(_device, view, nullptr);
            }
        });
    }

    _deletionStack.push([=]() mutable {
        // Destroy depth image
        vkDestroyImageView(_device, depth.view, nullptr);
        vmaDestroyImage(_allocator, depth.allocImage.image, depth.allocImage.allocation);
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

    { // Diffuse texture descriptor set (push descriptor set)
        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            // Diffuse texture
            vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0), 
            // Bump map
            vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
        };

        VkDescriptorSetLayoutCreateInfo diffuseSetInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            // This is a push descriptor set !
            .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
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

        static int i = 0;
        setDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, f.cmd, "Command buffer. Frame " + std::to_string(i));
        ++i;


        // Synchronization primitives
        VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();
        VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.imageAvailableSemaphore));
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.renderFinishedSemaphore));

        VKASSERT(vkCreateFence(_device, &fenceInfo, nullptr, &f.inFlightFence));
    }

    {
        { // Camera + scene + mat buffers descriptor set
            f.cameraBuffer = createBuffer(sizeof(GPUCameraUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            f.sceneBuffer  = createBuffer(sizeof(GPUSceneUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            //f.matBuffer    = createBuffer(sizeof(GPUMatUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            f.objectBuffer = createBuffer(sizeof(GPUSceneSSBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            // Image descriptor for the shadow cube map
            _shadow.cubemapArray.allocImage.descInfo = {
                .sampler = _shadow.sampler,
                .imageView = _shadow.cubemapArray.view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };

            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.cameraBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // Camera UB
                .bind_buffer(1, &f.sceneBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // Scene UB
                //.bind_buffer(2, &f.matBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // Material UB
                .bind_buffer(2, &f.objectBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT) // Objects SSBO
                .bind_image (3, nullptr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Skybox cubemap sampler (Skybox will be passed later when loaded)
                .bind_image (4, &_shadow.cubemapArray.allocImage.descInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Shadow cubemap sampler
                .build(f.globalSet, _globalSetLayout);
        }

        { // Shadow pass descriptor set
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.sceneBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                //.bind_buffer(0, &f.shadowUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                .bind_buffer(1, &f.objectBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // SSBO
                .build(f.shadowPassSet, _shadowSetLayout);
        }

        { // Compute descriptor sets
            f.compSSBO = createBuffer(sizeof(GPUCompSSBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            f.compUB = createBuffer(sizeof(GPUCompUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            // Create layout and write descriptor set for COMPUTE histogram step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_buffer(1, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_image(2, nullptr, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image (imageInfo bound later, every frame)
                .build(f.compHistogramSet, _compute.histogram.setLayout);

            // Create layout and write descriptor set for COMPUTE average luminance step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_buffer(1, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .build(f.compAvgLumSet, _compute.averageLuminance.setLayout);

            // Create layout and write descriptor set for COMPUTE tone mapping step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_buffer(1, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_image(2, nullptr, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image (imageInfo bound later, every frame)
                .build(f.compTonemapSet, _compute.toneMapping.setLayout);
        }
    }

    _deletionStack.push([&]() { 
        f.cameraBuffer.destroy(_allocator);
        f.sceneBuffer.destroy(_allocator);
        //f.matBuffer.destroy(_allocator);

        f.objectBuffer.destroy(_allocator);

        f.compSSBO.destroy(_allocator);
        f.compUB.destroy(_allocator);

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

void Engine::setDebugName(VkObjectType type, void* handle, const std::string name)
{
    VkDebugUtilsObjectNameInfoEXT name_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = type,
        .objectHandle = (uint64_t)handle,
        .pObjectName = name.c_str()
    };
    vkSetDebugUtilsObjectNameEXT(_device, &name_info);
}