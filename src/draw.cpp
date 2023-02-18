#include "stdafx.h"
#include "Enigne.h"

void Engine::drawFrame()
{
    VKASSERT(vkWaitForFences(_device, 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX));
    VKASSERT(vkResetFences(_device, 1, &_inFlightFences[_currentFrame]));

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    } else {
        ASSERTMSG(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "failed to acquire swap chain image!");
    }

    VKASSERT(vkResetCommandBuffer(_commandBuffers[_currentFrame], 0));
    recordCommandBuffer(_commandBuffers[_currentFrame], imageIndex);

    VkSemaphore waitSemaphores[] = { _imageAvailableSemaphores[_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore signalSemaphores[] = { _renderFinishedSemaphores[_currentFrame] };

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &_commandBuffers[_currentFrame],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores
    };

    VKASSERT(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _inFlightFences[_currentFrame]));

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

    _currentFrame = (++_frameNumber) % MAX_FRAMES_IN_FLIGHT;
}

void Engine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo = vkinit::command_buffer_begin_info();

    VKASSERT(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    //Clear-color from frame number. This will flash with a 120*pi frame period.
    VkClearValue clearValue;

    auto flash = [](uint32_t frame, float t) { return abs(sin(frame / t)); };
    float val = 0.1f;
    glm::vec3 color = { 
        flash(_frameNumber, 600.f), flash(_frameNumber, 1200.f), flash(_frameNumber, 2400.f) };
    color *= val;
    clearValue.color = { { color.x, color.y, color.z, 1.0f } };

    VkRenderPassBeginInfo renderPassBeginInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = _renderPass,
        .framebuffer = _swapchainFramebuffers[imageIndex],
        .renderArea = {
            .offset = { 0, 0 },
            .extent = _swapchainExtent
        },
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline);
    {
        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)_swapchainExtent.width,
            .height = (float)_swapchainExtent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{
            .offset = { 0, 0 },
            .extent = _swapchainExtent
        };

        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }
    {
        VkDeviceSize zeroOffset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &_triangleMesh.vertexBuffer.buffer, &zeroOffset);
    }
    {
        float camZ = -5.f;
        glm::vec3 camPos{ 0.f, 0.f, camZ };
        glm::mat4 viewMat = glm::translate(glm::mat4(1.f), camPos);
        glm::mat4 projMat = glm::perspective(glm::radians(45.f), _swapchainExtent.width / (float)_swapchainExtent.height, 0.1f, 10.f);
        projMat[1][1] *= -1; //Flip y-axis
        float rotSpeed = 0.1f;
        glm::mat4 modelMat = glm::rotate(glm::mat4(1.f), glm::radians(_frameNumber * rotSpeed), glm::vec3(0, 1, 0));
        glm::mat4 mvpMat = projMat * viewMat * modelMat;
        MeshPushConstants pushConstants{ .render_matrix = mvpMat };
        vkCmdPushConstants(commandBuffer, _pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &pushConstants);
    }
    vkCmdDraw(commandBuffer, uint32_t(_triangleMesh.vertices.size()), 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    VKASSERT(vkEndCommandBuffer(commandBuffer));
}