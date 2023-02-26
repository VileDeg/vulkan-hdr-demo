#include "stdafx.h"
#include "Enigne.h"

void Engine::createFramebuffers()
{
    size_t viewCount = _swapchainImageViews.size();
    _swapchainFramebuffers.resize(viewCount);

    VkFramebufferCreateInfo framebufferInfo = vkinit::framebuffer_create_info(_renderPass, _windowExtent);

    for (size_t i = 0; i < viewCount; i++)
    {
        std::vector<VkImageView> attachments = {
            _swapchainImageViews[i],
            _depthImageView
        };

        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();

        VKASSERT(vkCreateFramebuffer(_device, &framebufferInfo, nullptr, &_swapchainFramebuffers[i]));
    }
}
