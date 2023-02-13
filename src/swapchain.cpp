#include "stdafx.h"
#include "Enigne.h"

void Engine::pickSurfaceFormat()
{
    vkGetDeviceQueue(_device, _graphicsQueueFamily, 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, _presentQueueFamily, 0, &_presentQueue);

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    VKASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physicalDevice, _surface, &surfaceCapabilities));

    uint32_t formatCount;
    VKASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &formatCount, nullptr));
    std::vector<VkSurfaceFormatKHR> availableSurfaceFormats(formatCount);
    VKASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(_physicalDevice, _surface, &formatCount, availableSurfaceFormats.data()));

    // print surface formats
#ifndef NDEBUG
    std::cout << "Surface formats:" << std::endl;
    for (VkSurfaceFormatKHR sf : availableSurfaceFormats)
        std::cout << "\t" << string_VkFormat(sf.format) << ", color space: " << string_VkColorSpaceKHR(sf.colorSpace) << std::endl;
#endif // NDEBUG

    // choose surface format
    constexpr const std::array allowedSurfaceFormats{
        VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_SRGB, VK_COLORSPACE_SRGB_NONLINEAR_KHR },
        VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_SRGB , VK_COLORSPACE_SRGB_NONLINEAR_KHR },
        VkSurfaceFormatKHR{ VK_FORMAT_A8B8G8R8_SRGB_PACK32, VK_COLORSPACE_SRGB_NONLINEAR_KHR }
    };
    if (availableSurfaceFormats.size() == 1 && availableSurfaceFormats[0].format == VK_FORMAT_UNDEFINED)
        // Vulkan spec allowed single eUndefined value until 1.1.111 (2019-06-10)
        // with the meaning you can use any valid vk::Format value.
        // Now, it is forbidden, but let's handle any old driver.
        _surfaceFormat = allowedSurfaceFormats[0];
    else {
        for (VkSurfaceFormatKHR sf : availableSurfaceFormats) {
            for (VkSurfaceFormatKHR allowed : allowedSurfaceFormats) {
                if (sf.format == allowed.format && sf.colorSpace == allowed.colorSpace) {
                    _surfaceFormat = sf;
                    goto surfaceFormatFound;
                }
            }
        }
        // Vulkan must return at least one format (this is mandated since Vulkan 1.0.37 (2016-10-10), but was missing in the spec before probably because of omission)
        ASSERTMSG(availableSurfaceFormats.size() != 0, "Vulkan error: getSurfaceFormatsKHR() returned empty list.");
        _surfaceFormat = availableSurfaceFormats[0];
    surfaceFormatFound:;
    }
#ifndef NDEBUG
    std::cout << "Using format:\n"
        << "\t" << string_VkFormat(_surfaceFormat.format) << ", color space: "
        << string_VkColorSpaceKHR(_surfaceFormat.colorSpace) << std::endl;
#endif // NDEBUG
}