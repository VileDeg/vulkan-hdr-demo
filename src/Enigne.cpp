#include "stdafx.h"

#include "Enigne.h"
#include "utils.h"

#include "defs.h"

void Engine::Init()
{
    initWindow();
    initInstance();
    initDebugMessenger();
    initSurface();
    initPhysicalDevice();
    initLogicalDevice();
    initSwapchain();
}

void Engine::Draw()
{
}

void Engine::Run()
{
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
    }
}

void Engine::initWindow()
{
    glfwInit();

    glfwSetErrorCallback(glfw_error_callback);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWASSERTMSG(glfwVulkanSupported(),
        "GLFW: Vulkan Not Supported");

    _window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
}

void Engine::initInstance()
{
    _instanceExtensions = get_required_extensions();

    ASSERTMSG(!ENABLE_VALIDATION_LAYERS || checkValidationLayerSupport(_enabledValidationLayers),
        "Not all requested validation layers are available!");
    
    VKASSERT(vkCreateInstance(
        HCCP(VkInstanceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = HCCP(VkApplicationInfo){
                .pApplicationName = "Demo",
                .applicationVersion = 1,
                .apiVersion = VK_MAKE_VERSION(1, 0, 0)
            },
            .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? (uint32_t)_enabledValidationLayers.size() : 0,
            .ppEnabledLayerNames = ENABLE_VALIDATION_LAYERS ? _enabledValidationLayers.data() : nullptr,
            .enabledExtensionCount = (uint32_t)_instanceExtensions.size(),
            .ppEnabledExtensionNames = _instanceExtensions.data()
        }, 
        nullptr, &_instance)
    );

    
}

void Engine::initDebugMessenger()
{
    DYNAMIC_LOAD(cdum, _instance, vkCreateDebugUtilsMessengerEXT);

    if (ENABLE_VALIDATION_LAYERS) {
        VKASSERT(cdum(_instance,
            HCCP(VkDebugUtilsMessengerCreateInfoEXT){
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                .pfnUserCallback = debug_callback
        },
            nullptr, & _debugMessenger)
        );
    }
}

void Engine::initSurface()
{
    VKASSERTMSG(glfwCreateWindowSurface(_instance, _window, nullptr, &_surface),
        "GLFW: Failed to create window surface");
}

void Engine::initPhysicalDevice()
{
    auto result = pickPhysicalDevice();
    ASSERTMSG(result.has_value(), "No compatible devices found.");

    auto&& [pd, gqf, pqf] = result.value();
    _physicalDevice = pd;
    _graphicsQueueFamily = gqf;
    _presentQueueFamily = pqf;
}

void Engine::initLogicalDevice()
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {
        VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = _graphicsQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &(const float&)1.0f
        },
        VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = _presentQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &(const float&)1.0f
        },
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = _graphicsQueueFamily == _presentQueueFamily ? uint32_t(1) : static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? (uint32_t)_enabledValidationLayers.size() : 0,
        .ppEnabledLayerNames = ENABLE_VALIDATION_LAYERS ? _enabledValidationLayers.data() : nullptr,
        .enabledExtensionCount = (uint32_t)_deviceExtensions.size(),
        .ppEnabledExtensionNames = _deviceExtensions.data()
    };

    VKASSERT(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device));
}

void Engine::initSwapchain()
{
    pickSurfaceFormat();
}


void Engine::Cleanup()
{
    vkDestroyDevice(_device, nullptr);

    if (ENABLE_VALIDATION_LAYERS) {
        DYNAMIC_LOAD(ddum, _instance, vkDestroyDebugUtilsMessengerEXT);
        ddum(_instance, _debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyInstance(_instance, nullptr);

    glfwDestroyWindow(_window);
    glfwTerminate();
}