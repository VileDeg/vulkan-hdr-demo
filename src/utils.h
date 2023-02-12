#pragma once

#include "Enigne.h"

static bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers) {
    const auto& availableLayerProperties = vk::enumerateInstanceLayerProperties();

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

static bool checkInstanceExtensionSupport(const std::vector<const char*>& requiredExtensions) {
    const auto& availableExtensionProperties = vk::enumerateInstanceExtensionProperties();
#ifndef NDEBUG
    std::cout << "Available extensions: " << std::endl;
    for (const auto& extensionProperty : availableExtensionProperties) {
        std::cout << "\t" << extensionProperty.extensionName << std::endl;
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

static std::vector<const char*> get_required_extensions() {
    std::vector<const char*> requiredExtensions{};

    uint32_t glfwExtCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    for (uint32_t i = 0; i < glfwExtCount; i++) {
        requiredExtensions.emplace_back(glfwExtensions[i]);
    }
    if (Engine::enableValidationLayers) {
        requiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (!checkInstanceExtensionSupport(requiredExtensions)) {
        throw std::runtime_error("No instance extensions properties are available.");
    }

    return requiredExtensions;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}


static void glfw_error_callback(int error, const char* description)
{
    std::cout << "GLFW Error: " << description << std::endl;
}