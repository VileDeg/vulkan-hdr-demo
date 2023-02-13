#include "stdafx.h"

#include "Enigne.h"
#include "utils.h"

void Engine::Init()
{
    initWindow();
    initInstance();
    initPhysicalDevice();
    initLogicalDevice();
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

    if (glfwVulkanSupported == GLFW_FALSE) {
        throw std::runtime_error("GLFW: Vulkan Not Supported");
    }

    _window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
}

void Engine::initInstance()
{
    enabledExtensions = get_required_extensions();

    if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport(enabledValidationLayers)) {
        throw std::runtime_error("Vulkan: Not all requested validation layers are available!");
    }

    _instance = vk::createInstanceUnique(
        vk::InstanceCreateInfo{
            .pApplicationInfo = &(const vk::ApplicationInfo&)vk::ApplicationInfo{
                .pApplicationName = "Demo",
                .applicationVersion = 1,
                .apiVersion = VK_MAKE_VERSION(1, 0, 0)
            },
            .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? (uint32_t)enabledValidationLayers.size() : 0,
            .ppEnabledLayerNames = ENABLE_VALIDATION_LAYERS ? enabledValidationLayers.data() : nullptr,
            .enabledExtensionCount = (uint32_t)enabledExtensions.size(),
            .ppEnabledExtensionNames = enabledExtensions.data()
        }
    );

    /*if (enableValidationLayers) {
        _debugMessanger = _instance->createDebugUtilsMessengerEXTUnique(
            vk::DebugUtilsMessengerCreateInfoEXT{
                .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
                .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
                .pfnUserCallback = debugCallback
            }
        );
    }*/

    
}


void Engine::initPhysicalDevice()
{
    VkSurfaceKHR tmp;
    if (glfwCreateWindowSurface(_instance.get(), _window, nullptr, &tmp) != VK_SUCCESS) {
        throw std::runtime_error("GLFW: Failed to create window surface");
    }
    _surface = vk::UniqueSurfaceKHR(tmp, _instance.get());

    if (auto result = pickPhysicalDevice(_instance.get(), _surface.get())) {
        auto&& [pd, gqf, pqf] = result.value();
        _physicalDevice = pd;
        _graphicsQueueFamily = gqf;
        _presentationQueueFamily = pqf;
    } else {
        throw std::runtime_error("No compatible devices found.");
    }
}

void Engine::initLogicalDevice()
{
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos = {
        vk::DeviceQueueCreateInfo{
            .queueFamilyIndex = _graphicsQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &(const float&)1.0f
        },
        vk::DeviceQueueCreateInfo{
            .queueFamilyIndex = _presentationQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &(const float&)1.0f
        },
        
    };
    vk::DeviceCreateInfo deviceCreateInfo = vk::DeviceCreateInfo{
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? (uint32_t)enabledValidationLayers.size() : 0,
        .ppEnabledLayerNames = ENABLE_VALIDATION_LAYERS ? enabledValidationLayers.data() : nullptr,
    };


}
