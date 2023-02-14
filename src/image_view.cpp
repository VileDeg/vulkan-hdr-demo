#include "stdafx.h"
#include "Enigne.h"

void Engine::createImageViews()
{
    _swapchainImageViews.resize(_swapchainImages.size());

    for (size_t i = 0; i < _swapchainImages.size(); ++i) {
        VkImageViewCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = _swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = _swapchainImageFormat,
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

        VKASSERT(vkCreateImageView(_device, &createInfo, nullptr, &_swapchainImageViews[i]));
    }
}