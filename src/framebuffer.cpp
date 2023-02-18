#include "stdafx.h"
#include "Enigne.h"

void Engine::createFramebuffers()
{
    size_t viewCount = _swapchainImageViews.size();
    _swapchainFramebuffers.resize(viewCount);

    VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_renderPass, _swapchainExtent);

    for (size_t i = 0; i < viewCount; i++)
    {
        framebufferInfo.pAttachments = &_swapchainImageViews[i];

        VKASSERT(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapchainFramebuffers[i]));
    }
}
