#include "stdafx.h"

#include "Engine.h"

/**
* Check if all required extenstions are supported by Vulkan instance.
*/
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

/**
* Check if all required layers are available.
*/
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

/**
* Get extension names required by GLFW and validation layers.
* Then check if all required extensions are supported by Vulkan instance.
*/
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

/**
* Create Vulkan instance and check for validation layers support.
*/
void Engine::createInstance()
{
    _instanceExtensions = get_required_extensions();

    ASSERTMSG(!ENABLE_VALIDATION_LAYERS || checkValidationLayerSupport(_enabledValidationLayers),
        "Not all requested validation layers are available!");

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan Demo",
        .applicationVersion = 1,
        .apiVersion = VK_MAKE_VERSION(1, 0, 0)
    };

    VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? (uint32_t)_enabledValidationLayers.size() : 0,
        .ppEnabledLayerNames = ENABLE_VALIDATION_LAYERS ? _enabledValidationLayers.data() : nullptr,
        .enabledExtensionCount = (uint32_t)_instanceExtensions.size(),
        .ppEnabledExtensionNames = _instanceExtensions.data()
    };

    VKASSERT(vkCreateInstance(&instanceInfo, nullptr, & _instance));
    {
        _deletionStack.push([&]() { vkDestroyInstance(_instance, nullptr); });
    }
}

/**
* Create window surface through GLFW.
*/
void Engine::createSurface()
{
    VKASSERTMSG(glfwCreateWindowSurface(_instance, _window, nullptr, &_surface),
        "GLFW: Failed to create window surface");

    _deletionStack.push([&]() { vkDestroySurfaceKHR(_instance, _surface, nullptr); });
}
