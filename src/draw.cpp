#include "stdafx.h"
#include "Engine.h"

void Engine::drawFrame()
{
    imguiOnDrawStart();

    _frameInFlightNum = (_frameNumber) % MAX_FRAMES_IN_FLIGHT;
    FrameData& frame = _frames[_frameInFlightNum];

    VKASSERT(vkWaitForFences(_device, 1, &frame.inFlightFence, VK_TRUE, UINT64_MAX));
    VKASSERT(vkResetFences(_device, 1, &frame.inFlightFence));

    //Get index of next image after 
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, frame.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    } else {
        ASSERTMSG(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "failed to acquire swap chain image!");
    }

    VKASSERT(vkResetCommandBuffer(frame.mainCmdBuffer, 0));
    recordCommandBuffer(frame.mainCmdBuffer, imageIndex);

    VkSemaphore waitSemaphores[] = { frame.imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { frame.renderFinishedSemaphore };

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame.mainCmdBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores
    };

    VKASSERT(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, frame.inFlightFence));

    VkSwapchainKHR swapchains[] = { _swapchain };
    VkPresentInfoKHR presentInfo{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signalSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &imageIndex
    };

    result = vkQueuePresentKHR(_presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _framebufferResized) {
        _framebufferResized = false;
        recreateSwapchain();
    }
    else {
        VKASSERTMSG(result, "failed to present swap chain image!");
    }

    ++_frameNumber;
}

void Engine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info();

    VKASSERT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    VkClearValue colorClear;
    {
        /*auto flash = [](uint32_t frame, float t) { return abs(sin(frame / t)); };
        float val = 0.05f;
        glm::vec3 color = {
            flash(_frameNumber, 600.f), flash(_frameNumber, 1200.f), flash(_frameNumber, 2400.f) };
        color *= val;*/
        //colorClear.color = { color.x, color.y, color.z, 1.0f };
        colorClear.color = { 0.1f, 0.0f, 0.1f, 1.0f };
    }

    VkClearValue depthClear;
    {
        depthClear.depthStencil = { 1.0f, 0 };
    }

    VkClearValue clearValues[] = { colorClear, depthClear };

    VkRenderPassBeginInfo renderPassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = _renderPass,
        .framebuffer = _swapchainFramebuffers[imageIndex],
        .renderArea = {
            .offset = { 0, 0 },
            .extent = _windowExtent
        },
        .clearValueCount = 2,
        .pClearValues = clearValues
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    drawObjects(commandBuffer, _renderables);

    // Record dear imgui primitives into command buffer
    imguiOnRenderPassEnd(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    VKASSERT(vkEndCommandBuffer(commandBuffer));
}