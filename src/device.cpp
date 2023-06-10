#include "stdafx.h"
#include "Engine.h"

static bool checkRequiredFeaturesSupport(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2& deviceFeatures)
{
    // Check if the device supports the shaderDrawParameters feature
    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);
    auto shaderDrawParametersFeatures = reinterpret_cast<VkPhysicalDeviceShaderDrawParametersFeatures*>(deviceFeatures.pNext);
    return shaderDrawParametersFeatures->shaderDrawParameters;
}

void Engine::createLogicalDevice()
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{
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

    // Needed for gl_BaseIndex
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES
    };

    // Enable nullDescriptor to pass descriptors with no image view
    VkPhysicalDeviceRobustness2FeaturesEXT robust2features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = &shaderDrawParametersFeatures,
        .nullDescriptor = VK_TRUE
    };
    
    VkPhysicalDeviceFeatures2 deviceFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &robust2features
    };

    if (!checkRequiredFeaturesSupport(_physicalDevice, deviceFeatures)) {
        throw std::runtime_error("Physcial device does not support required features");
    }

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures,
        .queueCreateInfoCount = _graphicsQueueFamily == _presentQueueFamily ? uint32_t(1) : uint32_t(2),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? (uint32_t)_enabledValidationLayers.size() : 0,
        .ppEnabledLayerNames = ENABLE_VALIDATION_LAYERS ? _enabledValidationLayers.data() : nullptr,
        .enabledExtensionCount = (uint32_t)_deviceExtensions.size(),
        .ppEnabledExtensionNames = _deviceExtensions.data()
    };

    VKASSERT(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device));

    _deletionStack.push([&]() { vkDestroyDevice(_device, nullptr); });

    vkGetDeviceQueue(_device, _graphicsQueueFamily, 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, _presentQueueFamily, 0, &_presentQueue);

    // Load push descriptor command for future use
    DYNAMIC_LOAD(vkCmdPushDescriptorSetKHR, _instance, vkCmdPushDescriptorSetKHR);
}
