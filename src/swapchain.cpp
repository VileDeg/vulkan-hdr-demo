#include "stdafx.h"
#include "engine.h"
#include "vulkan/vk_enum_string_helper.h"

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
    ASSERT_MSG(formatCount != 0, "No surface formats supported");
    support.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, support.formats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    ASSERT_MSG(presentModeCount != 0, "No present modes supported");
    support.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, support.presentModes.data());

    return support;
}

static VkSurfaceFormatKHR pickSurfaceFormat(VkPhysicalDevice physical_device, VkSurfaceKHR surface, 
    const std::vector<VkSurfaceFormatKHR>& request_formats)
{
    // Based on: "external/imgui/imgui_impl_vulkan.h", function: "ImGui_ImplVulkanH_SelectSurfaceFormat"
    /*The MIT License (MIT)

    Copyright (c) 2014-2024 Omar Cornut

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.*/

    static bool first_time_created = true;
    

    ASSERT(request_formats.size() > 0);

    // Per Spec Format and View Format are expected to be the same unless VK_IMAGE_CREATE_MUTABLE_BIT was set at image creation
    // Assuming that the default behavior is without setting this bit, there is no need for separate Swapchain image and image view format
    // Additionally several new color spaces were introduced with Vulkan Spec v1.0.40,
    // hence we must make sure that a format with the mostly available color space, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, is found and used.
    uint32_t avail_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &avail_count, nullptr);
    std::vector<VkSurfaceFormatKHR> avail_format;
    avail_format.resize((int)avail_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &avail_count, avail_format.data());

    if (first_time_created) {
        std::cout << "Available surface formats:\n";
        for (const auto& af : avail_format) {
            auto surface_format_name = string_VkFormat(af.format);
            auto color_space_name = string_VkColorSpaceKHR(af.colorSpace);

            std::cout << "\t" << surface_format_name << "    " << color_space_name << "\n";
        }
        std::cout << "\n";
    }

    // First check if only one format, VK_FORMAT_UNDEFINED, is available, which would imply that any format is available
    if (avail_count == 1)
    {
        if (avail_format[0].format == VK_FORMAT_UNDEFINED)
        {
            VkSurfaceFormatKHR ret;
            ret.format = request_formats[0].format;
            ret.colorSpace = request_formats[0].colorSpace;
            return ret;
        } else {
            // Pick the only available one
            return avail_format[0];
        }
    } else {
        int selected_format_index = -1;

        // Request several formats, the first found will be used
        for (int request_i = 0; request_i < request_formats.size(); request_i++) {

            for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++) { // Look for format
                if ((
                        request_formats[request_i].format == VK_FORMAT_UNDEFINED || 
                        avail_format[avail_i].format      == request_formats[request_i].format
                    ) &&
                    avail_format[avail_i].colorSpace == request_formats[request_i].colorSpace
                )
                {
                    selected_format_index = avail_i;
                    goto break_out;
                }
            }
        }

break_out:
        ASSERT(selected_format_index != -1);

        VkSurfaceFormatKHR selected_format = avail_format[selected_format_index];

        if (first_time_created) {
            auto surface_format_name = string_VkFormat(selected_format.format);
            auto color_space_name = string_VkColorSpaceKHR(selected_format.colorSpace);

            std::cout << "Selected surface format:\n";
            std::cout << "\t" << surface_format_name << "    " << color_space_name << "\n";
            std::cout << "\n";

            first_time_created = false;
        }

        // If none of the requested image formats could be found, use the first available
        return selected_format;
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

    // Reference : 
    // https://github.com/nxp-imx/gtec-demo-framework/blob/master/DemoApps/Vulkan/HDR04_HDRFramebuffer/source/HDR04_HDRFramebuffer_Register.cpp

    // Select Surface Format
    static const std::vector<VkSurfaceFormatKHR> requestSurfaceImageFormat = {
        // HDR formats are prioritized. At least one of them should be supported on any HDR device
        // VK_FORMAT_UNDEFINED is used as a placeholder for any available format
        // it is supposed that device only supports combinations of VkFromat and VkColorSpace that make sense. 
        { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_HDR10_ST2084_EXT }, 
        { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT }, 
        { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT },
        
        { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT }, 
        { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_DCI_P3_LINEAR_EXT },
        { VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT },

        // Prefered SDR formats: 
        // BT.2020 has a wide color gamut
        { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_BT2020_LINEAR_EXT },

        { VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },

        // Combination of linear format and nonlinear color space are not prefered as they require different gamma settings 
        // and light calculations in shaders become incorrect. 
        { VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
        { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR },
    };
    
    
    auto surfaceFormat = pickSurfaceFormat(
        _physicalDevice, _surface, requestSurfaceImageFormat); 

    _swapchain.colorFormat = surfaceFormat.format;

    auto presentMode = pickPresentMode(support.presentModes);
    VkExtent2D extent = pickExtent(support.capabilities, _window);
    _swapchain.width = extent.width;
    _swapchain.height = extent.height;

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
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = _swapchain.handle
    };

    VkSwapchainKHR newSwapchain{};
    VK_ASSERT(vkCreateSwapchainKHR(_device, &swapchainInfo, nullptr, &newSwapchain));
 
    //Because we are using the old swapchain to create the new one, 
    //we only delete it after the new one is created
    if (_swapchain.handle != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(_device, _swapchain.handle, nullptr);
    }
    _swapchain.handle = newSwapchain;

    vkGetSwapchainImagesKHR(_device, _swapchain.handle, &imageCount, nullptr);
    _swapchain.images.resize(imageCount);
    vkGetSwapchainImagesKHR(_device, _swapchain.handle, &imageCount, _swapchain.images.data());

    for (int i = 0; i < _swapchain.images.size(); ++i) {
        setDebugName(VK_OBJECT_TYPE_IMAGE, _swapchain.images[i], "Swapchain Image " + std::to_string(i));
    }
}

void Engine::prepareSwapchainPass() {
    VkExtent3D imageExtent3D = {
        _swapchain.width,
        _swapchain.height,
        1 
    };

    _swapchain.imageViews.resize(_swapchain.images.size());

    for (size_t i = 0; i < _swapchain.images.size(); ++i) {
        //Image view
        VkImageViewCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = _swapchain.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = _swapchain.colorFormat,
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

        VK_ASSERT(vkCreateImageView(_device, &createInfo, nullptr, &_swapchain.imageViews[i]));
        setDebugName(VK_OBJECT_TYPE_IMAGE_VIEW, _swapchain.imageViews[i], "Swapchain Image View " + std::to_string(i));
    }
}


void Engine::recreateSwapchain()
{
    vkDeviceWaitIdle(_device);

    int width, height;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    cleanupSwapchainResources();

    createSwapchain();
    prepareSwapchainPass();
}

void Engine::cleanupSwapchainResources()
{
    for (auto& imageView : _swapchain.imageViews) {
        vkDestroyImageView(_device, imageView, nullptr);
    }
}
