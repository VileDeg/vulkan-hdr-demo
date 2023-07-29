#include "stdafx.h"
#include "engine.h"

std::string Engine::ASSET_PATH = "assets/";
std::string Engine::SHADER_PATH = ASSET_PATH + "shaders/bin/";
std::string Engine::IMAGE_PATH = ASSET_PATH + "images/";
std::string Engine::MODEL_PATH = ASSET_PATH + "models/";
std::string Engine::SCENE_PATH = ASSET_PATH + "scenes/";

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
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME, // Push descriptor to load diffuse texture per each mesh. TODO: remove? because not supported by all drivers
        VK_KHR_MAINTENANCE_4_EXTENSION_NAME,
        VK_EXT_ROBUSTNESS_2_EXTENSION_NAME,

        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, // Required by dynamic rendering

        VK_KHR_MAINTENANCE_2_EXTENSION_NAME, // Required by Renderpass2
        VK_KHR_MULTIVIEW_EXTENSION_NAME, // Required by Renderpass2
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, // Required by dynamic rendering

        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // To get rid of renderpasses and framebuffers

        //VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME // To update desc sets after binding
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
    
    prepareMainPass();
    prepareViewportPass(_swapchain.imageExtent.width, _swapchain.imageExtent.height);
    prepareShadowPass();

    initDescriptors();

    createFrameData();
    createPipelines();
    
    loadScene(SCENE_PATH + "dobrovic-sponza.json");
    
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

    VKASSERT(vkCreateImageView(_device, &view_info, nullptr, &att.view));

    utils::setDebugName(_device, VK_OBJECT_TYPE_IMAGE, att.allocImage.image, "IMAGE: " + debugName);
    utils::setDebugName(_device, VK_OBJECT_TYPE_IMAGE_VIEW, att.view, "IMAGE_VIEW: " + debugName);

    att.allocImage.descInfo = {
        .sampler = _linearSampler,
        .imageView = att.view,
        .imageLayout = layout
    };

    immediate_submit([&](VkCommandBuffer cmd) {
        utils::setImageLayout(cmd, att.allocImage.image, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout);
    });
}

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
        VKASSERT(vmaCreateImage(_allocator, &dimg_info, &dimg_allocinfo, &_viewport.images[i].image, &_viewport.images[i].allocation, nullptr));
        utils::setDebugName(_device, VK_OBJECT_TYPE_IMAGE, _viewport.images[i].image, "Viewport Image " + std::to_string(i));

        VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_viewport.colorFormat, _viewport.images[i].image, VK_IMAGE_ASPECT_COLOR_BIT);

        VKASSERT(vkCreateImageView(_device, &dview_info, nullptr, &_viewport.imageViews[i]));
        utils::setDebugName(_device, VK_OBJECT_TYPE_IMAGE_VIEW, _viewport.imageViews[i], "Viewport Image View " + std::to_string(i));
    }

    { // Compute attachments
        for (int i = 0; i < ComputeStagesLTM::MAX_LTM_ATTACMENTS; ++i) {
            createAttachment(
                _viewport.colorFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                extent3D, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                _compute.ltm.att[i], "COMPUTE_ATTACHMENT_" + std::to_string(i)
            );
        }
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

    { // Compute attachments
        for (int i = 0; i < ComputeStagesLTM::MAX_LTM_ATTACMENTS; ++i) {
            _compute.ltm.att[i].Cleanup(_device, _allocator);
        }
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

    bool validDepthFormat = utils::getSupportedDepthFormat(_physicalDevice, &_shadow.depthFormat);
    ASSERTMSG(validDepthFormat, "Physical device has no supported depth formats");

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
            VKASSERT(vkCreateImageView(_device, &view, nullptr, &faceViews[face]));
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


void Engine::initFrame(FrameData& f, int frame_i)
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


        utils::setDebugName(_device, VK_OBJECT_TYPE_COMMAND_BUFFER, f.cmd, "Command buffer. Frame " + std::to_string(frame_i));



        // Synchronization primitives
        VkSemaphoreCreateInfo semaphoreInfo = vkinit::semaphore_create_info();
        VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.imageAvailableSemaphore));
        VKASSERT(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &f.renderFinishedSemaphore));

        VKASSERT(vkCreateFence(_device, &fenceInfo, nullptr, &f.inFlightFence));
    }

    {
        { // Camera + scene + mat buffers descriptor set
            f.cameraBuffer = allocateBuffer(sizeof(GPUCameraUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            f.sceneBuffer = allocateBuffer(sizeof(GPUSceneUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            //f.matBuffer    = createBuffer(sizeof(GPUMatUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            f.objectBuffer = allocateBuffer(sizeof(GPUSceneSSBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

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
                .bind_image(3, nullptr, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Skybox cubemap sampler (Skybox will be passed later when loaded)
                .bind_image(4, &_shadow.cubemapArray.allocImage.descInfo, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // Shadow cubemap sampler
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
            f.compSSBO = allocateBuffer(sizeof(GPUCompSSBO), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
            f.compUB = allocateBuffer(sizeof(GPUCompUB), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

            // Create layout and write descriptor set for COMPUTE histogram step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_buffer(1, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_image(2, &_viewport.images[0].descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image
                .build(_compute.histogram.sets[frame_i], _compute.histogram.setLayout);

            // Create layout and write descriptor set for COMPUTE average luminance step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_buffer(1, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .build(_compute.averageLuminance.sets[frame_i], _compute.averageLuminance.setLayout);

            { // Create layout and write descriptor set for COMPUTE LTM step
                // 0 - lum, 1 - chrom, 2 - base
                vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                    .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                    .bind_buffer(1, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                    // In
                    .bind_image(2, &_viewport.images[0].descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image
                    // Out
                    .bind_image(3, &_compute.ltm.att[0].allocImage.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Out Image
                    .bind_image(4, &_compute.ltm.att[1].allocImage.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Out Image

                    .build(_compute.ltm.stages[0].sets[frame_i], _compute.ltm.stages[0].setLayout);

                vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                    .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                    .bind_buffer(1, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                    // In
                    .bind_image(2, &_compute.ltm.att[0].allocImage.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image
                    // Out
                    .bind_image(3, &_compute.ltm.att[2].allocImage.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Out Image
                    .bind_image(4, &_compute.ltm.att[3].allocImage.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Out Image

                    .build(_compute.ltm.stages[1].sets[frame_i], _compute.ltm.stages[1].setLayout);

                vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                    .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                    .bind_buffer(1, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                    // In
                    .bind_image(2, &_compute.ltm.att[0].allocImage.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image
                    .bind_image(3, &_compute.ltm.att[1].allocImage.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Out Image
                    .bind_image(4, &_compute.ltm.att[2].allocImage.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Out Image
                    .bind_image(5, &_compute.ltm.att[3].allocImage.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Out Image
                    // Out
                    .bind_image(6, &_viewport.images[0].descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Out Image

                    .build(_compute.ltm.stages[2].sets[frame_i], _compute.ltm.stages[2].setLayout);
            }

            // Create layout and write descriptor set for COMPUTE tone mapping step
            vkutil::DescriptorBuilder::begin(_descriptorLayoutCache, _descriptorAllocator)
                .bind_buffer(0, &f.compSSBO.descInfo, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_buffer(1, &f.compUB.descInfo, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT) // SSBO for luminance histogram data
                .bind_image(2, &_viewport.images[0].descInfo, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT) // Viewport HDR image (imageInfo bound later, every frame)
                .build(_compute.toneMapping.sets[frame_i], _compute.toneMapping.setLayout);
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

void Engine::createFrameData()
{
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        initFrame(_frames[i], i);
    }
}

