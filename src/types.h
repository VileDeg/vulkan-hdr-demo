#pragma once

struct DeletionStack {
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
    /* Based on https://github.com/vblanco20-1/vulkan-guide */
    VkBuffer buffer;
    VmaAllocation allocation;

    void runOnMemoryMap(VmaAllocator allocator, std::function<void(void*)> func) {
        void* data;
        vmaMapMemory(allocator, allocation, &data);
        func(data);
        vmaUnmapMemory(allocator, allocation);
    }

    void destroy(const VmaAllocator& allocator) {
        vmaDestroyBuffer(allocator, buffer, allocation);
    }
};

struct AllocatedImage {
    /* Based on https://github.com/vblanco20-1/vulkan-guide */
    VkImage image;
    VmaAllocation allocation;

    void destroy(VmaAllocator& allocator) {
        vmaDestroyImage(allocator, image, allocation);
    }
};

struct FrameData {
    VkSemaphore imageAvailableSemaphore, renderFinishedSemaphore;
    VkFence inFlightFence;

    VkCommandPool commandPool;

    VkCommandBuffer mainCmdBuffer;
    VkCommandBuffer viewportCmdBuffer;

    AllocatedBuffer cameraBuffer;
    AllocatedBuffer objectBuffer;

    VkDescriptorSet globalDescriptor;
    VkDescriptorSet objectDescriptor;

    void cleanup(const VkDevice& device, const VmaAllocator& allocator);
};



