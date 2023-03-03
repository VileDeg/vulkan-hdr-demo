#pragma once

struct DeletionStack
{
    std::stack<std::function<void()>> deletors;

    void push(std::function<void()>&& function) {
        deletors.push(function);
    }

    void flush() {
        while (!deletors.empty()) {
            deletors.top()();
            deletors.pop();
        }
    }
};

struct ShaderData {
    std::vector<char> code;
    VkShaderModule module;
};

struct PipelineShaders {
    ShaderData vert;
    ShaderData frag;

    void cleanup(const VkDevice& device) {
        vkDestroyShaderModule(device, vert.module, nullptr);
        vkDestroyShaderModule(device, frag.module, nullptr);
    }
};

struct AllocatedBuffer {
    VkBuffer buffer;
    VmaAllocation allocation;

    void cleanup(const VmaAllocator& allocator) {
        vmaDestroyBuffer(allocator, buffer, allocation);
    }
};

struct AllocatedImage {
    VkImage image;
    VmaAllocation allocation;
};

struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 render_matrix;
};

struct GPUCameraData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
};

struct FrameData {
    VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
    VkFence inFlightFence;

    VkCommandPool commandPool;
    VkCommandBuffer mainCmdBuffer;

    AllocatedBuffer cameraBuffer;
    VkDescriptorSet globalDescriptor;

    void cleanup(const VkDevice& device, const VmaAllocator& allocator) {
        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyCommandPool(device, commandPool, nullptr);

        cameraBuffer.cleanup(allocator);
    }
};;