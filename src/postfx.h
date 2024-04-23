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

struct PostFXStage {
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

    void Create(VkDevice device, VkSampler sampler,
        const std::string& shaderBinName, bool usePushConstants = false);
    void Destroy();

    PostFXStage& Bind(VkCommandBuffer cmd);

    PostFXStage& UpdateImage(VkImageView view, uint32_t binding);
    PostFXStage& UpdateImage(Attachment att, uint32_t binding);

    PostFXStage& UpdateImagePyramid(AttachmentPyramid& att, uint32_t binding);

    PostFXStage& WriteDescriptorSets(int set_i);

    PostFXStage& Dispatch(uint32_t groupsX, uint32_t groupsY, int set_i);
    void Barrier();

    void InitDescriptorSets(
        DescriptorLayoutCache* dLayoutCache, DescriptorAllocator* dAllocator,
        FrameData& f, int frame_i);
};

enum Effect {
    INVALID = -1, EXPADP, DURAND, FUSION, BLOOM, GTMO, GAMMA
};

enum class GAMMA_MODE {
    INVALID = -1, OFF, ON, INVERSE
};

struct PostFX {
    enum class LTM : int {
        DURAND = 0, FUSION
    };

    PostFX();

    void UpdateStagesDescriptorSets(int numOfFrames, const std::vector<VkImageView>& viewportImageViews);

    PostFXStage& Stage(Effect fct, std::string key);
    Attachment& Att(Effect fct, std::string key);
    AttachmentPyramid& Pyr(Effect fct, std::string key);

    std::string getPrefixFromEffect(Effect fct);
    Effect getEffectFromPrefix(std::string pref);

    bool isEffectEnabled(Effect fct);

    void setGammaMode(GAMMA_MODE mode);

    std::map<std::string, PostFXStage> stages;
    std::map<std::string, Attachment> att;
    std::map<std::string, AttachmentPyramid> pyr;

    std::map<Effect, std::string> effectPrefixMap;

    GPUCompUB ub{};

    int numOfBloomMips;
    float lumPixelLowerBound;
    float lumPixelUpperBound;

    float eyeAdaptationTimeCoefficient;

    LTM localToneMappingMode = LTM::DURAND; // 0 - Durand2002, 1 - Exposure fusion

    float gamma;
    GAMMA_MODE gammaMode; // 0 - off, 1 - on, 2 - inverse

    bool enableBloom;
    bool enableGlobalToneMapping;
    bool enableAdaptation;
    bool enableLocalToneMapping;
};
