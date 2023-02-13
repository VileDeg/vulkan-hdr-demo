#include "stdafx.h"

#include "Enigne.h"

bool Engine::checkValidationLayerSupport(const std::vector<const char*>& requiredLayers) {

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

bool Engine::checkInstanceExtensionSupport(const std::vector<const char*>& requiredExtensions) {
    uint32_t propertyCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensionProperties(propertyCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, availableExtensionProperties.data());

#ifndef NDEBUG
    std::cout << "Available extensions: " << std::endl;
    for (const auto& extensionProperty : availableExtensionProperties) {
        std::cout << "\t" << extensionProperty.extensionName << std::endl;
    }
    std::cout << "Required extensions: " << std::endl;
    for (const auto& reqExt : requiredExtensions) {
        std::cout << "\t" << reqExt << std::endl;
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

std::vector<const char*> Engine::get_required_extensions() {
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
