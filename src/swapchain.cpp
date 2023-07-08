#include "stdafx.h"
#include "engine.h"

struct SwapchainPropertiesSupport {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

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

static SwapchainPropertiesSupport querySwapchainPropertiesSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapchainPropertiesSupport support;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    ASSERTMSG(formatCount != 0, "No surface formats supported");
    support.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, support.formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    ASSERTMSG(presentModeCount != 0, "No present modes supported");
    support.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, support.presentModes.data());

    return support;
}

static VkSurfaceFormatKHR pickSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceKHR surface, const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space)
{
    // Copied from file: "imgui/imgui_impl_vulkan.h", function: "ImGui_ImplVulkanH_SelectSurfaceFormat"

    ASSERT(request_formats != nullptr);
    ASSERT(request_formats_count > 0);

    // Per Spec Format and View Format are expected to be the same unless VK_IMAGE_CREATE_MUTABLE_BIT was set at image creation
    // Assuming that the default behavior is without setting this bit, there is no need for separate Swapchain image and image view format
    // Additionally several new color spaces were introduced with Vulkan Spec v1.0.40,
    // hence we must make sure that a format with the mostly available color space, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, is found and used.
    uint32_t avail_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &avail_count, nullptr);
    std::vector<VkSurfaceFormatKHR> avail_format;
    avail_format.resize((int)avail_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &avail_count, avail_format.data());

    // First check if only one format, VK_FORMAT_UNDEFINED, is available, which would imply that any format is available
    if (avail_count == 1)
    {
        if (avail_format[0].format == VK_FORMAT_UNDEFINED)
        {
            VkSurfaceFormatKHR ret;
            ret.format = request_formats[0];
            ret.colorSpace = request_color_space;
            return ret;
        } else
        {
            // No point in searching another format
            return avail_format[0];
        }
    } else
    {
        // Request several formats, the first found will be used
        for (int request_i = 0; request_i < request_formats_count; request_i++)
            for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
                if (avail_format[avail_i].format == request_formats[request_i] && avail_format[avail_i].colorSpace == request_color_space)
                    return avail_format[avail_i];

        // If none of the requested image formats could be found, use the first available
        return avail_format[0];
    }
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


void Engine::createSwapchain()
{
    const auto support = querySwapchainPropertiesSupport(_physicalDevice, _surface);

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    auto surfaceFormat = pickSurfaceFormat(_physicalDevice, _surface, requestSurfaceImageFormat, (size_t)ARRAY_SIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    auto presentMode = pickPresentMode(support.presentModes);
    _swapchain.imageExtent = pickExtent(support.capabilities, _window);

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
        .imageExtent = _swapchain.imageExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // Added sampled
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = _swapchainHandle
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
 
    //Because we are using the old swapchain to create the new one, 
    //we only delete it after the new one is created
    if (_swapchainHandle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_device, _swapchainHandle, nullptr);
    }
    _swapchainHandle = newSwapchain;

    vkGetSwapchainImagesKHR(_device, _swapchainHandle, &imageCount, nullptr);
    _swapchain.images.resize(imageCount);
    vkGetSwapchainImagesKHR(_device, _swapchainHandle, &imageCount, _swapchain.images.data());

    _swapchain.imageFormat = surfaceFormat.format;
}

void Engine::prepareMainPass() {
    VkExtent3D imageExtent3D = {
        _swapchain.imageExtent.width,
        _swapchain.imageExtent.height,
        1 
    };

    //Create depth image
    {
        VkImageCreateInfo dimgInfo = vkinit::image_create_info(_swapchain.depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, imageExtent3D);

        VmaAllocationCreateInfo dimgAllocinfo = {
            .usage = VMA_MEMORY_USAGE_GPU_ONLY,
            .requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        vmaCreateImage(_allocator, &dimgInfo, &dimgAllocinfo, &_swapchain.depthImage.image, &_swapchain.depthImage.allocation, nullptr);

        VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_swapchain.depthFormat, _swapchain.depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

        VKASSERT(vkCreateImageView(_device, &dview_info, nullptr, &_swapchain.depthImageView));
    }

    _swapchain.imageViews.resize(_swapchain.images.size());
    _swapchain.framebuffers.resize(_swapchain.images.size());

    VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_swapchain.renderpass, _swapchain.imageExtent);

    for (size_t i = 0; i < _swapchain.images.size(); ++i) {
        //Image view
        VkImageViewCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = _swapchain.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = _swapchain.imageFormat,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };

        VKASSERT(vkCreateImageView(_device, &createInfo, nullptr, &_swapchain.imageViews[i]));

        // Framebuffer
        std::vector<VkImageView> attachments = {
            _swapchain.imageViews[i],
            _swapchain.depthImageView
        };

        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();

        VKASSERT(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapchain.framebuffers[i]));
    }
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

    cleanupSwapchainResources();

    createSwapchain();
    prepareMainPass();
}

void Engine::cleanupSwapchainResources()
{
    for (auto& framebuffer : _swapchain.framebuffers) {
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    }

    //Destroy depth image
    vkDestroyImageView(_device, _swapchain.depthImageView, nullptr);
    vmaDestroyImage(_allocator, _swapchain.depthImage.image, _swapchain.depthImage.allocation);

    for (auto& imageView : _swapchain.imageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }
}


