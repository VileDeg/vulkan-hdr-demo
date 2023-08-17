#include "stdafx.h"
#include "engine.h"

Engine::Engine()
{
    _enabledValidationLayers = {
#if ENABLE_VALIDATION == 1
        "VK_LAYER_KHRONOS_validation",
#if ENABLE_VALIDATION_SYNC == 1
        "VK_LAYER_KHRONOS_synchronization2"
#endif
#endif
    };

    _instanceExtensions = {
#if ENABLE_VALIDATION == 1
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, // Required by dynamic rendering
    };

    _deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, // Swapchain to present images on screen
        VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,

        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, // Required by dynamic rendering

        VK_KHR_MAINTENANCE_2_EXTENSION_NAME, // Required by Renderpass2
        VK_KHR_MULTIVIEW_EXTENSION_NAME, // Required by Renderpass2
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, // Required by dynamic rendering

        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // To get rid of renderpasses and framebuffers

        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME // To update desc sets after binding and to allow unused descriptors to remain invalid
    };

     WIDTH = 1600;
     HEIGHT = 900;
     
     FS_WIDTH = 1920;
     FS_HEIGHT = 1080;
     ENABLE_FULLSCREEN = false;
}

void Engine::Init()
{
    createWindow();
    createInstance();
    createSurface();

    pickPhysicalDevice();
    createLogicalDevice();

    createVmaAllocator();

    createSamplers();
    initUploadContext();
    
    createSwapchain();

    prepareSwapchainPass();
    prepareViewportPass(_swapchain.width, _swapchain.height);
    prepareShadowPass();
    
    createFrameData();
    createPipelines();
    
    loadScene(SCENE_PATH + "dobrovic-sponza.json");
    
    ui_Init();

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

        if (_wasViewportResized) {
            // Need to wait for all commands to finish so that we can safely recreate and reregister all viewport images
            vkDeviceWaitIdle(_device);
            ui_UnregisterTextures();

            // Create viewport images and etc. with new extent
            recreateViewport(newViewportSizeX, newViewportSizeY);

            ui_RegisterTextures();

            _wasViewportResized = false;
        }
    }
}

void Engine::Cleanup()
{
    if (!_isInitialized) {
        return;
    }

    vkDeviceWaitIdle(_device);

    cleanupViewportResources();
    cleanupSwapchainResources();

    _descriptorAllocator->cleanup();
    _descriptorLayoutCache->cleanup();

    vkDestroySwapchainKHR(_device, _swapchain.handle, nullptr);

    _deletionStack.flush();
}


void Engine::createAttachment(
    VkFormat format, VkImageUsageFlags usage, 
    VkExtent3D extent, VkImageAspectFlags aspect,
    VkImageLayout layout,
    Attachment& att,
    const std::string debugName)
{
    VkImageCreateInfo imgInfo = vkinit::image_create_info(format, usage, extent);

    VmaAllocationCreateInfo imgAllocinfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    vmaCreateImage(_allocator, &imgInfo, &imgAllocinfo, &att.allocImage.image, &att.allocImage.allocation, nullptr);

    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, att.allocImage.image, aspect);

    VK_ASSERT(vkCreateImageView(_device, &view_info, nullptr, &att.view));

    setDebugName(VK_OBJECT_TYPE_IMAGE, att.allocImage.image, "ATTACHMENT::IMAGE::" + debugName);
    setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, att.view, "ATTACHMENT::VIEW::" + debugName);

    att.allocImage.descInfo = {
        .sampler = _linearSampler,
        .imageView = att.view,
        .imageLayout = layout
    };

    att.tag = debugName;
    att.dim = { extent.width, extent.height };

    immediate_submit([&](VkCommandBuffer cmd) {
        vk_utils::setImageLayout(cmd, att.allocImage.image, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout);
    });
}

void Engine::createAttachmentPyramid(
    VkFormat format, VkImageUsageFlags usage,
    VkExtent3D extent, VkImageAspectFlags aspect,
    VkImageLayout layout,
    uint32_t numOfMipLevels,
    AttachmentPyramid& att,
    const std::string debugName)
{
    // Create image with specified number of mips
    VkImageCreateInfo imgInfo = vkinit::image_create_info(format, usage, extent, numOfMipLevels);

    VmaAllocationCreateInfo imgAllocinfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    vmaCreateImage(_allocator, &imgInfo, &imgAllocinfo, &att.allocImage.image, &att.allocImage.allocation, nullptr);

    setDebugName(VK_OBJECT_TYPE_IMAGE, att.allocImage.image, "ATTACHMENT::IMAGE::" + debugName);

    // Create image view for each mip level
    for (uint32_t i = 0; i < numOfMipLevels; ++i) {
        VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, att.allocImage.image, aspect, i);

        VkImageView view;
        VK_ASSERT(vkCreateImageView(_device, &view_info, nullptr, &view));
        setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, view, "ATTACHMENT::VIEW::" + debugName);

        att.views.push_back(view);
    }

    att.tag = debugName;
    att.dim = { extent.width, extent.height };

    /*att.allocImage.descInfo = {
        .sampler = _linearSampler,
        .imageView = att.view,
        .imageLayout = layout
    };*/

    immediate_submit([&](VkCommandBuffer cmd) {
        vk_utils::setImageLayout(cmd, att.allocImage.image, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout);
        });
}

void Engine::prepareViewportPass(uint32_t extentX, uint32_t extentY) {
    _viewport.width = extentX;
    _viewport.height = extentY;

    _viewport.aspectRatio = (float)extentX / extentY;

    bool anyFormats = vk_utils::getSupportedDepthFormat(_physicalDevice, &_viewport.depthFormat);
    ASSERT_MSG(anyFormats, "Physical device has no supported depth formats");
    
    VkExtent3D extent3D = {
        extentX,
        extentY,
        1
    };

    //Create depth image
    {
        createAttachment(
            _viewport.depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            extent3D, VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            _viewport.depth, "VIEWPORT_DEPTH"
        );
    }

    size_t imgCount = _swapchain.images.size();
    _viewport.images.resize(imgCount);

    _viewport.imageViews.resize(imgCount);

    VkImageCreateInfo img_info = {
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

    VmaAllocationCreateInfo img_allocinfo = {};
    img_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    for (uint32_t i = 0; i < imgCount; i++)
    {
        //allocate and create the image
        VK_ASSERT(vmaCreateImage(_allocator, &img_info, &img_allocinfo, &_viewport.images[i].image, &_viewport.images[i].allocation, nullptr));
        setDebugName(VK_OBJECT_TYPE_IMAGE, _viewport.images[i].image, "Viewport Image " + std::to_string(i));

        VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_viewport.colorFormat, _viewport.images[i].image, VK_IMAGE_ASPECT_COLOR_BIT);

        VK_ASSERT(vkCreateImageView(_device, &dview_info, nullptr, &_viewport.imageViews[i]));
        setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, _viewport.imageViews[i], "Viewport Image View " + std::to_string(i));
    }

    { // Compute attachments
        // As defined in Merten's matlab implementation: https://github.com/Mericam/exposure-fusion
        int numOfViewportMips = std::floor(log(std::min(extentX, extentY) / log(2)));

        _postfx.ub.numOfViewportMips = numOfViewportMips;
        _postfx.numOfBloomMips = numOfViewportMips-2;

        for (auto& att : _postfx.att) {
            createAttachment(
                _viewport.colorFormat,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                extent3D, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                att.second, "COMPUTE::" + att.first
            );
        }
    
        for (auto& att : _postfx.pyr) {
            createAttachmentPyramid(
                _viewport.colorFormat,
                VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                extent3D, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                numOfViewportMips,
                att.second, "COMPUTE::" + att.first
            );
        }

        // First time this functions is called compute stages where not yet created
        if (_isInitialized) {
            updatePostFXStagesDescriptorSets();
        }
    }
}

void Engine::updatePostFXStagesDescriptorSets()
{
    int i = 0;
    for (auto& f : _frames) {
        auto& cp = _postfx;
        int imageIndex = i;

        { // Durand
            _postfx.Stage(DURAND, "lum_chrom")
                // In
                .UpdateImage(_viewport.imageViews[imageIndex], 1)
                // Out
                .UpdateImage(cp.Att(DURAND, "lum").view, 2)
                .UpdateImage(cp.Att(DURAND, "chrom").view, 3)

                .WriteSets(imageIndex);

            _postfx.Stage(DURAND, "bilateral")
                // In
                .UpdateImage(cp.Att(DURAND, "lum").view, 1)
                // Out
                .UpdateImage(cp.Att(DURAND, "base").view, 2)
                .UpdateImage(cp.Att(DURAND, "detail").view, 3)

                .WriteSets(imageIndex);

            _postfx.Stage(DURAND, "reconstruct")
                // In
                .UpdateImage(cp.Att(DURAND, "lum").view, 1)
                .UpdateImage(cp.Att(DURAND, "chrom").view, 2)
                .UpdateImage(cp.Att(DURAND, "base").view, 3)
                .UpdateImage(cp.Att(DURAND, "detail").view, 4)
                // Out
                .UpdateImage(_viewport.imageViews[imageIndex], 5)

                .WriteSets(imageIndex);
        }

        { // Bloom
            cp.Stage(BLOOM, "threshold")
                .UpdateImage(_viewport.imageViews[imageIndex], 1)
                .UpdateImage(cp.Pyr(BLOOM, "highlights").views[0], 2)
                .WriteSets(imageIndex);

            cp.Stage(BLOOM, "downsample")
                .UpdateImagePyramid(cp.Pyr(BLOOM, "highlights"), 0)
                .WriteSets(imageIndex);

            cp.Stage(BLOOM, "upsample")
                .UpdateImagePyramid(cp.Pyr(BLOOM, "highlights"), 0)
                .WriteSets(imageIndex);

            cp.Stage(BLOOM, "combine")
                .UpdateImage(_viewport.imageViews[imageIndex], 1)
                .UpdateImage(cp.Pyr(BLOOM, "highlights").views[0], 2)
                .WriteSets(imageIndex);
        }

        { // Exposure fusion
            cp.Stage(FUSION, "lum_chrom_weight")
                // Out
                .UpdateImage(_viewport.imageViews[i], 1)
                .UpdateImage(cp.Att(FUSION, "chrom"), 2)
                .UpdateImage(cp.Pyr(FUSION, "lum").views[0], 3)
                .UpdateImage(cp.Pyr(FUSION, "weight").views[0], 4)
                .WriteSets(imageIndex);

            cp.Stage(FUSION, "downsample")
                // In
                .UpdateImagePyramid(cp.Pyr(FUSION, "lum"), 0)
                // Out
                .UpdateImagePyramid(cp.Pyr(FUSION, "weight"), 1)
                .WriteSets(imageIndex);


            int last_i = _postfx.ub.numOfViewportMips - 1;
            cp.Stage(FUSION, "residual")
                // In
                .UpdateImage(cp.Pyr(FUSION, "lum").views[last_i], 0)
                .UpdateImage(cp.Pyr(FUSION, "weight").views[last_i], 1)
                // Out
                .UpdateImage(cp.Pyr(FUSION, "blendedLaplac").views[last_i], 2)
                .WriteSets(imageIndex);


            cp.Stage(FUSION, "upsample0_sub")
                // In
                .UpdateImagePyramid(cp.Pyr(FUSION, "lum"), 0)
                // Out
                .UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 1)

                .WriteSets(imageIndex);

            cp.Stage(FUSION, "upsample1_sub")
                // In
                .UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 0)
                .UpdateImagePyramid(cp.Pyr(FUSION, "lum"), 1)
                .UpdateImagePyramid(cp.Pyr(FUSION, "weight"), 2)
                // Out
                .UpdateImagePyramid(cp.Pyr(FUSION, "blendedLaplac"), 3)
                .WriteSets(imageIndex);

            cp.Stage(FUSION, "upsample0_add")
                // In
                .UpdateImagePyramid(cp.Pyr(FUSION, "blendedLaplac"), 0)
                // Out
                .UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 1)
                .WriteSets(imageIndex);

            cp.Stage(FUSION, "upsample1_add")
                // In
                .UpdateImagePyramid(cp.Pyr(FUSION, "upsampled0"), 0)
                // Out
                .UpdateImagePyramid(cp.Pyr(FUSION, "blendedLaplac"), 1)
                .WriteSets(imageIndex);

            cp.Stage(FUSION, "reconstruct")
                // In
                .UpdateImage(cp.Att(FUSION, "chrom"), 1)

                .UpdateImage(cp.Pyr(FUSION, "blendedLaplac").views[0], 2)
                // Out
                .UpdateImage(_viewport.imageViews[imageIndex], 3)

                .WriteSets(imageIndex);
        }

        { // Exposure adaptation
            cp.Stage(EXPADP, "histogram")
                .UpdateImage(_viewport.imageViews[imageIndex], 2)
                .WriteSets(imageIndex);

            cp.Stage(EXPADP, "eyeadp")
                .UpdateImage(_viewport.imageViews[imageIndex], 1)
                .WriteSets(imageIndex);
        }

        { // Global tone mapping
            cp.Stage(GTMO, "0")
                .UpdateImage(_viewport.imageViews[imageIndex], 1)
                .WriteSets(imageIndex);
        }

        { // Gamma correction
            cp.Stage(GAMMA, "0")
                .UpdateImage(_viewport.imageViews[imageIndex], 1)
                .WriteSets(imageIndex);
        }

        ++i;
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
    //Destroy depth image
    _viewport.depth.Cleanup(_device, _allocator);

    for (auto& a : _postfx.att) {
        a.second.Cleanup(_device, _allocator);
    }

    for (auto& att : _postfx.pyr) {
        att.second.Cleanup(_device, _allocator);
    }


    for (auto& imageView : _viewport.imageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }

    _viewport.imageViews.clear();

    for (auto& image : _viewport.images) {
        vmaDestroyImage(_allocator, image.image, image.allocation);
    }

    _viewport.images.clear();
}


void Engine::prepareShadowPass()
{
    _shadow.width = ShadowPass::TEX_DIM;
    _shadow.height = ShadowPass::TEX_DIM;

    bool validDepthFormat = vk_utils::getSupportedDepthFormat(_physicalDevice, &_shadow.depthFormat);
    ASSERT_MSG(validDepthFormat, "Physical device has no supported depth formats");

    Attachment& cubemap = _shadow.cubemapArray;
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
    VmaAllocationCreateInfo imgAllocinfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    //create cubemap image
    VK_ASSERT(vmaCreateImage(_allocator, &imageCreateInfo, &imgAllocinfo,
        &cubemap.allocImage.image, &cubemap.allocImage.allocation, nullptr));

    // Image barrier for optimal image (target)
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = 1;
    subresourceRange.layerCount = cubeArrayLayerCount;

    immediate_submit([&](VkCommandBuffer cmd) {
        vk_utils::setImageLayout(
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
    VK_ASSERT(vkCreateImageView(_device, &arrayView, nullptr, &cubemap.view));

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
    VK_ASSERT(vkCreateSampler(_device, &sampler, nullptr, &_shadow.sampler));


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

        VK_ASSERT(vmaCreateImage(_allocator, &imageCreateInfo, &imgAllocinfo,
            &depth.allocImage.image, &depth.allocImage.allocation, nullptr));

        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (vk_utils::formatHasStencil(_shadow.depthFormat)) {
            aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        immediate_submit([&](VkCommandBuffer cmd) {
            vk_utils::setImageLayout(
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
        VK_ASSERT(vkCreateImageView(_device, &depthStencilView, nullptr, &depth.view));
    }

    VkImageView attachments[2];
    attachments[1] = depth.view;

    for (int i = 0; i < MAX_LIGHTS; ++i) {
        auto& faceViews = _shadow.faceViews[i];

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
            VK_ASSERT(vkCreateImageView(_device, &view, nullptr, &faceViews[face]));
        }

        // Cleanup all shadow pass resources for current light
        _deletionStack.push([=]() mutable {
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



void Engine::initUploadContext()
{
    VkFenceCreateInfo uploadFenceCreateInfo = vkinit::fence_create_info();
    VK_ASSERT(vkCreateFence(_device, &uploadFenceCreateInfo, nullptr, &_uploadContext.uploadFence));

    VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily);
    // Create pool for upload context
    VK_ASSERT(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr, &_uploadContext.commandPool));

    // Allocate the default command buffer that we will use for the instant commands
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_uploadContext.commandPool, 1);

    VK_ASSERT(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_uploadContext.commandBuffer));

    _deletionStack.push([&]() {
        vkDestroyCommandPool(_device, _uploadContext.commandPool, nullptr);
        vkDestroyFence(_device, _uploadContext.uploadFence, nullptr);
    });
}



void Engine::createFrameData()
{
    _descriptorAllocator = new DescriptorAllocator{};
    _descriptorAllocator->init(_device);

    _descriptorLayoutCache = new DescriptorLayoutCache{};
    _descriptorLayoutCache->init(_device);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        int frame_i = i;
        auto& f = _frames[i];

        {
            // Command pool
            VkCommandPoolCreateInfo poolInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                .queueFamilyIndex = _graphicsQueueFamily
            };

            VK_ASSERT(vkCreateCommandPool(_device, &poolInfo, nullptr, &f.commandPool));

            // Main command buffer
            VkCommandBufferAllocateInfo cmdBufferAllocInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = f.commandPool,
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1
            };

            VK_ASSERT(vkAllocateCommandBuffers(_device, &cmdBufferAllocInfo, &f.cmd));

            setDebugName(VK_OBJECT_TYPE_COMMAND_BUFFER, f.cmd, "Command buffer. Frame " + std::to_string(frame_i));

            // Synchronization primitives
            VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();
            VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

            VK_ASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.imageAvailableSemaphore));
            VK_ASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.renderFinishedSemaphore));

            VK_ASSERT(vkCreateFence(_device, &fenceInfo, nullptr, &f.inFlightFence));
        }

        {
            { // Camera + scene + mat buffers descriptor set
                f.cameraBuffer = allocateBuffer(sizeof(GPUCameraUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
                f.sceneBuffer = allocateBuffer(sizeof(GPUSceneUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
                f.objectBuffer = allocateBuffer(sizeof(GPUSceneSSBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

                // Image descriptor for the shadow cube map
                _shadow.cubemapArray.allocImage.descInfo = {
                    .sampler = _shadow.sampler,
                    .imageView = _shadow.cubemapArray.view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };

                DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                    .bind_buffer(0, &f.cameraBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // Camera UB
                    .bind_buffer(1, &f.sceneBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // Scene UB
                    .bind_buffer(2, &f.objectBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT) // Objects SSBO

                    .bind_image(3, nullptr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Skybox cubemap sampler (Skybox will be passed later when loaded)
                    .bind_image(4, &_shadow.cubemapArray.allocImage.descInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Shadow cubemap sampler

                    .bind_image_empty(5, MAX_TEXTURES, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Diffuse textures
                    .bind_image_empty(6, MAX_TEXTURES, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Bump textures

                    .build(f.sceneSet, _sceneSetLayout);

                setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, f.sceneSet, "DESCRIPTOR_SET::VIEWPORT_GLOBAL::FRAME_" + std::to_string(frame_i));
            }

            { // Shadow pass descriptor set
                DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                    .bind_buffer(0, &f.sceneBuffer.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                    //.bind_buffer(0, &f.shadowUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                    .bind_buffer(1, &f.objectBuffer.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // SSBO
                    .build(f.shadowPassSet, _shadowSetLayout);
                setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, f.shadowPassSet, "DESCRIPTOR_SET::SHADOW::FRAME_" + std::to_string(frame_i));
            }

            { // Compute descriptor sets
                f.compSSBO = allocateBuffer(sizeof(GPUCompSSBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
                f.compUB = allocateBuffer(sizeof(GPUCompUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

                for (auto& stage : _postfx.stages) {
                    stage.second.InitDescriptorSets(_descriptorLayoutCache, _descriptorAllocator, f, frame_i);
                    setDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, stage.second.sets[frame_i], "DESCRIPTOR_SET::COMPUTE::" + stage.first + "::FRAME_" + std::to_string(frame_i));
                }
            }
        }

        _deletionStack.push([&]() {
            f.cameraBuffer.destroy(_allocator);
            f.sceneBuffer.destroy(_allocator);

            f.objectBuffer.destroy(_allocator);

            f.compSSBO.destroy(_allocator);
            f.compUB.destroy(_allocator);

            vkDestroyFence(_device, f.inFlightFence, nullptr);

            vkDestroySemaphore(_device, f.imageAvailableSemaphore, nullptr);
            vkDestroySemaphore(_device, f.renderFinishedSemaphore, nullptr);

            vkDestroyCommandPool(_device, f.commandPool, nullptr);
        });
    }
}

#if ENABLE_VALIDATION == 1

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

void Engine::beginCmdDebugLabel(VkCommandBuffer cmd, std::string label, glm::vec4 color)
{
    VkDebugUtilsLabelEXT debugLabel = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
        .pLabelName = label.c_str(),
        .color = { color.r, color.g, color.b, color.a }
    };

    vkCmdBeginDebugUtilsLabelEXT(cmd, &debugLabel);
}

void Engine::beginCmdDebugLabel(VkCommandBuffer cmd, std::string label)
{
    static glm::vec4 initialColor = { 0.988, 0.678, 0.011, 1 };
    static glm::vec4 changeFactor = { 0.13, 0.0, 0.0, 0 };

    beginCmdDebugLabel(cmd, label, glm::abs(initialColor));
    initialColor = initialColor - changeFactor;

    // Wrap the values
    initialColor.r += initialColor.r < -1 ? 1 : 0;
    initialColor.b += initialColor.b < -1 ? 1 : 0;
    initialColor.g += initialColor.g < -1 ? 1 : 0;
}

void Engine::endCmdDebugLabel(VkCommandBuffer cmd)
{
    vkCmdEndDebugUtilsLabelEXT(cmd);
}

#else

void Engine::setDebugName(VkObjectType type, void* handle, const std::string name);
void Engine::beginCmdDebugLabel(VkCommandBuffer cmd, std::string label, glm::vec4 color);
void Engine::beginCmdDebugLabel(VkCommandBuffer cmd, std::string label);
void Engine::endCmdDebugLabel(VkCommandBuffer cmd);

#endif