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

    //int lastSetUpdateIndex = -1;

    void Create(VkDevice device, VkSampler sampler,
        const std::string& shaderBinName, bool usePushConstants = false);
    void Destroy();

    PostFXStage& Bind(VkCommandBuffer cmd);

    PostFXStage& UpdateImage(VkImageView view, uint32_t binding);
    PostFXStage& UpdateImage(Attachment att, uint32_t binding);

    PostFXStage& UpdateImagePyramid(AttachmentPyramid& att, uint32_t binding);

    PostFXStage& WriteSets(int set_i);

    PostFXStage& Dispatch(uint32_t groupsX, uint32_t groupsY, int set_i);
    void Barrier();

    void InitDescriptorSets(
        DescriptorLayoutCache* dLayoutCache, DescriptorAllocator* dAllocator,
        FrameData& f, int frame_i);
};

enum Effect {
    EXPADP, DURAND, FUSION, BLOOM, GTMO, GAMMA
};

struct PostFX {
    enum class LTM : int {
        DURAND = 0, FUSION
    };

    std::map<std::string, PostFXStage> stages;
    std::map<std::string, Attachment> att;
    std::map<std::string, AttachmentPyramid> pyr;

    PostFX();

    PostFXStage& Stage(Effect fct, std::string key);
    Attachment& Att(Effect fct, std::string key);
    AttachmentPyramid& Pyr(Effect fct, std::string key);

    std::string getPrefixFromEffect(Effect fct);
    Effect getEffectFromPrefix(std::string pref);

    bool isEffectEnabled(Effect fct);

    std::map<Effect, std::string> effectPrefixMap;

    GPUCompUB ub{};

    int numOfBloomMips;
    float lumPixelLowerBound = 0.2f;
    float lumPixelUpperBound = 0.95f;

    float maxLogLuminance = 4.7f;
    float eyeAdaptationTimeCoefficient = 2.2f;

    LTM localToneMappingMode = LTM::DURAND; // 0 - Durand2002, 1 - Exposure fusion

    float gamma = 2.2f;
    int gammaMode = 0; // 0 - forward, 1 - inverse

    int numOfBloomBlurPasses = 5;

    bool enableBloom = true;
    bool enableGlobalToneMapping = true;
    bool enableGammaCorrection = false;
    bool enableAdaptation = false;
    bool enableLocalToneMapping = false;
};
