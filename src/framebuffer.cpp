#include "stdafx.h"
#include "Enigne.h"

void Engine::createFramebuffers()
{
    size_t viewCount = _swapchainImageViews.size();
    _swapchainFramebuffers.resize(viewCount);

    VkFramebufferCreateInfo framebufferInfo{
           .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
           .renderPass = _renderPass,
           .attachmentCount = 1,
           
           .width = _swapchainExtent.width,
           .height = _swapchainExtent.height,
           .layers = 1
    };

    for (size_t i = 0; i < viewCount; i++)
    {
        framebufferInfo.pAttachments = &_swapchainImageViews[i];

        VKASSERT(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapchainFramebuffers[i]));
    }
}
