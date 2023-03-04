#include "stdafx.h"

#include "Enigne.h"

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

        DYNAMIC_LOAD(cdum, _instance, vkCreateDebugUtilsMessengerEXT);
        VKASSERT(cdum(_instance, &dbgMessengerInfo, nullptr, &_debugMessenger));
        
        _deletionStack.push([&]() { 
            DYNAMIC_LOAD(ddum, _instance, vkDestroyDebugUtilsMessengerEXT); 
            ddum(_instance, _debugMessenger, nullptr); 
        });
    }
}


