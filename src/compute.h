#pragma once

struct ComputeStageImageBinding {
    std::vector<VkImageView> views;
    uint32_t binding;
};

// Compute stage bindings
enum {
    UB, SSBO, IMG, PYR
};

// Forward declaration
class DescriptorLayoutCache;
class DescriptorAllocator;

struct ComputeStage {
    std::string tag = "";

    VkDescriptorSetLayout setLayout;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sets;

    std::vector<ComputeStageImageBinding> imageBindings;

    static constexpr int MAX_IMAGE_UPDATES = 64;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkDevice device;
    VkSampler sampler;

    std::string shaderName;
    std::vector<int> dsetBindings;
    bool usesPushConstants;

    //int lastSetUpdateIndex = -1;

    void Create(VkDevice device, VkSampler sampler,
        const std::string& shaderBinName, bool usePushConstants = false);
    void Destroy();

    ComputeStage& Bind(VkCommandBuffer cmd);

    ComputeStage& UpdateImage(VkImageView view, uint32_t binding);
    ComputeStage& UpdateImage(Attachment att, uint32_t binding);

    ComputeStage& UpdateImagePyramid(AttachmentPyramid& att, uint32_t binding);

    ComputeStage& WriteSets(int set_i);

    ComputeStage& Dispatch(uint32_t groupsX, uint32_t groupsY, int set_i);
    void Barrier();

    void InitDescriptorSets(
        DescriptorLayoutCache* dLayoutCache, DescriptorAllocator* dAllocator,
        FrameData& f, int frame_i);
};

enum Effect {
    EXPADP, DURAND, FUSION, BLOOM, GTMO, GAMMA
};

struct ComputePass {
    std::map<std::string, ComputeStage> stages;
    std::map<std::string, Attachment> att;
    std::map<std::string, AttachmentPyramid> pyr;

    ComputePass();

    ComputeStage& Stage(Effect fct, std::string key);
    Attachment& Att(Effect fct, std::string key);
    AttachmentPyramid& Pyr(Effect fct, std::string key);

    std::string getEffectPrefix(Effect fct);
};
