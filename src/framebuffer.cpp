#include "stdafx.h"
#include "Enigne.h"

void Engine::createFramebuffers()
{
    _swapchainFramebuffers.resize(_swapchainImageViews.size());

    for (size_t i = 0; i < _swapchainImageViews.size(); i++)
    {
        VkImageView attachments[] = {
            _swapchainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = _renderPass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = _swapchainExtent.width,
            .height = _swapchainExtent.height,
            .layers = 1
        };

        VKASSERT(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapchainFramebuffers[i]));
    }
}
