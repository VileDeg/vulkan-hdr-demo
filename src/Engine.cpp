#include "stdafx.h"

#include "Engine.h"
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

    _mainRenderpass     = createRenderPass(_device, _swapchain.imageFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, _swapchain.depthFormat);
    _viewportRenderpass = createRenderPass(_device, _swapchain.imageFormat, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, _swapchain.depthFormat);

    _deletionStack.push([&]() { vkDestroyRenderPass(_device, _mainRenderpass, nullptr); });
    _deletionStack.push([&]() { vkDestroyRenderPass(_device, _viewportRenderpass, nullptr); });

    createSwapchainImages();
    createViewportImages();

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

        imguiCommands();

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



void Engine::createViewportImages() {
    //Create depth image
    {
        VkExtent3D depthImageExtent = {
            _windowExtent.width,
            _windowExtent.height,
            1
        };

        _viewport.depthFormat = VK_FORMAT_D32_SFLOAT;

        VkImageCreateInfo dimgInfo = vkinit::image_create_info(_viewport.depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

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

    VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_viewportRenderpass, _windowExtent);

    for (uint32_t i = 0; i < imgCount; i++)
    {
        VkImageCreateInfo dimg_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = imageFormat, // Same formate as swapchain? TODO: correct format? VK_FORMAT_B8G8R8A8_SRGB
            .extent = { _windowExtent.width, _windowExtent.height, 1 }, // Extent to whole window
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // 
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };



        VmaAllocationCreateInfo dimg_allocinfo = {};
        dimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY; // was GPU only

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

void Engine::recreateViewport()
{
    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(_device);

    cleanupViewportResources();

    createViewportImages();
}

void Engine::cleanupViewportResources()
{
    for (auto& framebuffer : _viewport.framebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }

    //Destroy depth image
    vkDestroyImageView(_device, _viewport.depthImageView, nullptr);
    vmaDestroyImage(_allocator, _viewport.depthImage.image, _viewport.depthImage.allocation);

    for (auto& imageView : _viewport.imageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }

    for (auto& image : _viewport.images) {
        vmaDestroyImage(_allocator, image.image, image.allocation);
    }
}


VkRenderPass createRenderPass(VkDevice device, VkFormat colorAttFormat, VkImageLayout colorAttFinalLayout, VkFormat depthAttFormat)
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

    VkAttachmentReference colorAttachmentRef{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
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

    VkAttachmentReference depthAttachmentRef{
        .attachment = 1,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    };

    VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef,
        .pDepthStencilAttachment = &depthAttachmentRef
    };

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





static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void Engine::createDebugMessenger()
{
    if (ENABLE_VALIDATION_LAYERS) {
        VkDebugUtilsMessengerCreateInfoEXT dbgMessengerInfo{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            .pfnUserCallback = debug_callback
        };

        DYNAMIC_LOAD(vkCreateDebugUtilsMessengerEXT, _instance, vkCreateDebugUtilsMessengerEXT);
        DYNAMIC_LOAD(vkDestroyDebugUtilsMessengerEXT, _instance, vkDestroyDebugUtilsMessengerEXT);

        VKASSERT(vkCreateDebugUtilsMessengerEXT(_instance, &dbgMessengerInfo, nullptr, &_debugMessenger));

        _deletionStack.push([&]() {
            vkDestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
            });
    }
}

static bool checkInstanceExtensionSupport(const std::vector<const char*>& requiredExtensions) {
    uint32_t propertyCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensionProperties(propertyCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, availableExtensionProperties.data());

#ifndef NDEBUG
    pr("Available extensions: ");
    for (const auto& extensionProperty : availableExtensionProperties) {
        pr("\t" << extensionProperty.extensionName);
    }
    pr("Required extensions: ");
    for (const auto& reqExt : requiredExtensions) {
        pr("\t" << reqExt);
    }
#endif // NDEBUG

    for (const auto& requiredExtension : requiredExtensions) {
        bool extensionFound = false;

        for (const auto& extensionProperty : availableExtensionProperties) {
            if (strcmp(requiredExtension, extensionProperty.extensionName) == 0) {
                extensionFound = true;
                break;
            }
        }

        if (!extensionFound) {
            return false;
        }
    }

    return true;
}

static bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers) {

    uint32_t propertyCount = 0;
    vkEnumerateInstanceLayerProperties(&propertyCount, nullptr);
    std::vector<VkLayerProperties> availableLayerProperties(propertyCount);
    vkEnumerateInstanceLayerProperties(&propertyCount, availableLayerProperties.data());

    for (const char* requiredLayerName : requiredLayers) {
        bool layerFound = false;

        for (const auto& layerProperty : availableLayerProperties) {
            if (strcmp(requiredLayerName, layerProperty.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}


static std::vector<const char*> get_required_extensions() {
    std::vector<const char*> requiredExtensions{};

    uint32_t glfwExtCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    for (uint32_t i = 0; i < glfwExtCount; i++) {
        requiredExtensions.emplace_back(glfwExtensions[i]);
    }

    if (ENABLE_VALIDATION_LAYERS) {
        requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (!checkInstanceExtensionSupport(requiredExtensions)) {
        throw std::runtime_error("No instance extensions properties are available.");
    }

    return requiredExtensions;
}

void Engine::createInstance()
{
    _instanceExtensions = get_required_extensions();

    ASSERTMSG(!ENABLE_VALIDATION_LAYERS || checkValidationLayerSupport(_enabledValidationLayers),
        "Not all requested validation layers are available!");

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan HDR Demo",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_1
    };

    VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? (uint32_t)_enabledValidationLayers.size() : 0,
        .ppEnabledLayerNames = ENABLE_VALIDATION_LAYERS ? _enabledValidationLayers.data() : nullptr,
        .enabledExtensionCount = (uint32_t)_instanceExtensions.size(),
        .ppEnabledExtensionNames = _instanceExtensions.data()
    };

    VKASSERT(vkCreateInstance(&instanceInfo, nullptr, &_instance));
    {
        _deletionStack.push([&]() { vkDestroyInstance(_instance, nullptr); });
    }


}


void Engine::createSurface()
{
    VKASSERTMSG(glfwCreateWindowSurface(_instance, _window, nullptr, &_surface),
        "GLFW: Failed to create window surface");

    _deletionStack.push([&]() { vkDestroySurfaceKHR(_instance, _surface, nullptr); });
}


