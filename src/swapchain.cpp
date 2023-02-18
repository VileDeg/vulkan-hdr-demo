#include "stdafx.h"
#include "Enigne.h"

struct SwapchainPropertiesSupport {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

static SwapchainPropertiesSupport querySwapchainPropertiesSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapchainPropertiesSupport support;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    ASSERTMSG(formatCount != 0, "No surface formats supported");
    support.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, support.formats.data());

    // print surface formats
//#ifndef NDEBUG
//    std::cout << "Available surface formats:" << std::endl;
//    for (VkSurfaceFormatKHR sf : support.formats)
//        std::cout << "\t" << string_VkFormat(sf.format) << ", color space: " << string_VkColorSpaceKHR(sf.colorSpace) << std::endl;
//#endif // NDEBUG

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    ASSERTMSG(presentModeCount != 0, "No present modes supported");
    support.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, support.presentModes.data());

    // print present modes
//#ifndef NDEBUG
//    std::cout << "Available present modes:" << std::endl;
//    for (VkPresentModeKHR pm : support.presentModes)
//        std::cout << "\t" << string_VkPresentModeKHR(pm) << std::endl;
//#endif // NDEBUG

    return support;
}

static VkSurfaceFormatKHR pickSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& af : availableFormats) {
        if (af.format == VK_FORMAT_B8G8R8A8_SRGB && af.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return af;
        }
    }

    return availableFormats[0];
}

static VkPresentModeKHR pickPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (VkPresentModeKHR pm : availablePresentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            return pm;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D pickExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
{
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void Engine::createSwapchain()
{
    const auto support = querySwapchainPropertiesSupport(_physicalDevice, _surface);
    auto surfaceFormat = pickSurfaceFormat(support.formats);
    auto presentMode = pickPresentMode(support.presentModes);
    auto windowExtent = pickExtent(support.capabilities, _window);

    auto caps = support.capabilities;
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = _surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = windowExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = _swapchain
    };

    uint32_t queueFamilyIndices[] = { _graphicsQueueFamily, _presentQueueFamily };
    if (_graphicsQueueFamily != _presentQueueFamily) {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkSwapchainKHR newSwapchain{};
    VKASSERT(vkCreateSwapchainKHR(_device, &swapchainInfo, nullptr, &newSwapchain));
    if (_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    }
    _swapchain = newSwapchain;

    vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, nullptr);
    _swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(_device, _swapchain, &imageCount, _swapchainImages.data());

    _swapchainImageFormat = surfaceFormat.format;
    _swapchainExtent = windowExtent;
}

void Engine::recreateSwapchain()
{
    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(_device);

    for (auto& framebuffer : _swapchainFramebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }

    for (auto& imageView : _swapchainImageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }

    createSwapchain();
    createImageViews();
    createFramebuffers();
}

void Engine::cleanupSwapchain()
{
    for (auto& framebuffer : _swapchainFramebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }

    for (auto& imageView : _swapchainImageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }

    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}
