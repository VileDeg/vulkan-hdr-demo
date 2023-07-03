#pragma once

#include "Camera.h"
//#include "vk_descriptors.h"

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
    VkBuffer buffer;
    VmaAllocation allocation;
    VkDescriptorBufferInfo descInfo;

    void* gpu_ptr;

    bool hostVisible;

    /*void runOnMemoryMap(VmaAllocator allocator, std::function<void(void*)> func) {
        void* data;
        vmaMapMemory(allocator, allocation, &data);
        func(data);
        vmaUnmapMemory(allocator, allocation);
    }*/

    void create(VmaAllocator allocator, VkBufferCreateInfo bufferInfo, VmaAllocationCreateInfo allocInfo);
    void destroy(const VmaAllocator& allocator);
};

struct AllocatedImage {
    /* Based on https://github.com/vblanco20-1/vulkan-guide */
    VkImage image;
    VmaAllocation allocation;
    VkDescriptorImageInfo descInfo;
};

struct FrameData {
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

    VkSemaphore graphicsToComputeSemaphore;

    VkFence inFlightFence;

    VkCommandPool commandPool;

    VkCommandBuffer cmdBuffer;
    //VkCommandBuffer viewportCmdBuffer;

    AllocatedBuffer cameraBuffer;
    AllocatedBuffer objectBuffer;

    AllocatedBuffer compLumBuffer;

    VkDescriptorSet globalDescriptor;
    VkDescriptorSet objectDescriptor;

    VkDescriptorSet compLumDescriptor;

    //vkutil::DescriptorAllocator descriptorAllocator;
};

struct DescriptorSet {

};


/**
* Struct that holds data related to immediate command execution.
* This is used for uploading data to the GPU.
*/
struct UploadContext {
    VkFence uploadFence;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
};

struct InputContext {
    InputContext(std::function<void(void)> onFbResize)
        : onFramebufferResize(onFbResize) {}

    Camera camera = {};

    bool cursorEnabled = false;
    bool framebufferResized = false;

    bool toneMappingEnabled = true;
    bool exposureEnabled = true;

    std::function<void(void)> onFramebufferResize = nullptr;
};

struct Viewport {
    std::vector<AllocatedImage> images; // Allocated with VMA

    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    VkImageView depthImageView;
    AllocatedImage depthImage;
    VkFormat depthFormat;

    // Corresponds to dimensions of ImGui::Image viewport
    VkExtent2D imageExtent;
};

struct Swapchain {
    VkSwapchainKHR handle{ VK_NULL_HANDLE };

    std::vector<VkImage> images; // Retrieved from created swapchain

    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    VkFormat imageFormat;

    VkImageView depthImageView;
    AllocatedImage depthImage;
    VkFormat depthFormat;

    VkExtent2D imageExtent; // Corresponds to window dimensions
};
