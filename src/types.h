#pragma once

#include "Camera.h"

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

    VkCommandBuffer cmd;
    //VkCommandBuffer viewportCmdBuffer;

    AllocatedBuffer cameraBuffer;
    AllocatedBuffer objectBuffer;

    AllocatedBuffer compLumBuffer;

    VkDescriptorSet globalSet;
    VkDescriptorSet objectSet;

    VkDescriptorSet compHistogramSet;
    VkDescriptorSet compAvgLumSet;

    //vkutil::DescriptorAllocator descriptorAllocator;
};

struct ComputeParts {
    VkDescriptorSetLayout setLayout;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
};

struct Compute {
    ComputeParts histogram;
    ComputeParts averageLuminance;
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

struct RenderResources {
    std::vector<VkImage> images; // Retrieved from created swapchain

    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    VkFormat imageFormat;

    VkImageView depthImageView;
    AllocatedImage depthImage;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkExtent2D imageExtent; // Corresponds to window dimensions

    VkRenderPass renderpass;

    RenderResources() = default;
    RenderResources(VkFormat format) : imageFormat{ format } {}
};
