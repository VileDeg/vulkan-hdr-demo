#pragma once

#include "gpu_types.h"

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

struct Attachment {
    std::string tag = "";
    AllocatedImage allocImage;
    VkImageView view;
};

struct FrameData {
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;

    VkFence inFlightFence;

    VkCommandPool commandPool;
    VkCommandBuffer cmd;

    AllocatedBuffer cameraBuffer;
    AllocatedBuffer sceneBuffer;
    AllocatedBuffer objectBuffer;

    AllocatedBuffer compSSBO;
    AllocatedBuffer compUB;

    VkDescriptorSet globalSet;

    VkDescriptorSet compHistogramSet;
    VkDescriptorSet compAvgLumSet;
    VkDescriptorSet compBlurSet;
    VkDescriptorSet compTonemapSet;

    VkDescriptorSet shadowPassSet;
};

struct ComputeStage {
    VkDescriptorSetLayout setLayout;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
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

struct ViewportPass {
    std::vector<AllocatedImage> images; // Allocated with VMA
    std::vector<VkImageView> imageViews;

    // Rendering in 32-bit HDR format to allow further processing with compute shaders
    const VkFormat colorFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

    Attachment depth;

    VkFormat depthFormat;

    VkExtent2D imageExtent; // Viewport dimensions

    Attachment blur;
};

struct SwapchainPass {
    VkSwapchainKHR handle{ VK_NULL_HANDLE };

    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;

    VkFormat colorFormat;

    VkExtent2D imageExtent; // Window dimensions
};

struct ComputePass {
    ComputeStage histogram;
    ComputeStage averageLuminance;
    ComputeStage blur;
    ComputeStage toneMapping;
};

struct ShadowPass {
    // Texture properties
    static constexpr int TEX_DIM = 512; //1024
    static constexpr VkFilter TEX_FILTER = VK_FILTER_LINEAR;

    uint32_t width, height;

    Attachment cubemapArray;
    Attachment depth;

    std::array<std::array<VkImageView, 6>, MAX_LIGHTS> faceViews;

    VkSampler sampler;

    VkFormat colorFormat = VK_FORMAT_R32_SFLOAT;
    VkFormat depthFormat; // Will be picked from available formats
};

enum TextureDynamicRangeType {
    TEX_SDR, TEX_HDR
};

struct VertexInputDescription {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;
    glm::vec2 uv;

    static VertexInputDescription getDescription();

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && uv == other.uv;
    }
};

template <class T>
inline void hash_combine(std::size_t& s, const T& v)
{
    // Similar to boost::hash_combine
    std::hash<T> h;
    s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            std::size_t res = 0;
            hash_combine(res, vertex.pos);
            hash_combine(res, vertex.normal);
            hash_combine(res, vertex.color);
            hash_combine(res, vertex.uv);
            return res;
        }
    };
}

struct Material {
    std::string tag = "";
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};


struct Mesh {
    std::string tag = "";
    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    std::vector<uint32_t> indices;
    AllocatedBuffer indexBuffer;

    Attachment* diffuseTex{ nullptr };
    Attachment* bumpTex{ nullptr };

    int mat_id = -1;

    Material* material{ nullptr };

    GPUMaterial gpuMat{};
};

struct Model {
    std::string tag = "";
    std::vector<Mesh*> meshes;

    bool lightAffected = true;
    bool useObjectColor = false;

    float maxExtent{ 0.f };
};

struct RenderObject {
    std::string tag = "";
    glm::vec4 color{};

    Model* model;
    
    bool isSkybox = false;

    glm::vec3 pos{0.f};
    glm::vec3 rot{0.f};
    glm::vec3 scale{1.f};

    glm::mat4 Transform();
    bool HasMoved();

    glm::vec3 _prevPos{0.f};
};

// Struct with pointers to mapped GPU buffer memory
struct GPUData {
    GPUCameraUB* camera = nullptr;
    GPUSceneUB* scene = nullptr;
    GPUSceneSSBO* ssbo = nullptr;

    GPUCompSSBO* compSSBO = nullptr;
    GPUCompUB* compUB = nullptr;

    void Reset(FrameData& fd);
};

struct CreateSceneData {
    float intensity[MAX_LIGHTS];
    glm::vec3 position[MAX_LIGHTS];

    float bumpStrength;
    std::string modelPath;
    std::string skyboxPath;
};

struct RenderContext {
    GPUSceneUB sceneData{};
    GPUCompUB comp{};

    // Using shared ptr because it takes pointers of vector elements which is unsafe if vector gets resized
    std::vector<std::shared_ptr<RenderObject>> lightObjects;

    bool enableSkybox = true;
    bool displayLightSourceObjects = true;

    float zNear = 0.1f;
    float zFar = 64.0f;

    std::array<glm::mat4, 6> lightView;

    bool showNormals = false;

    float lumPixelLowerBound = 0.2f;
    float lumPixelUpperBound = 0.95f;

    float maxLogLuminance = 4.7f;
    float eyeAdaptationTimeCoefficient = 2.2f;

    // Treshold to calculate light's effective radius for optimization
    float lightRadiusTreshold = 1.f / 255.f;

    std::string modelName;
    std::string skyboxName;

    void Init(CreateSceneData data);

    void UpdateLightPosition(int lightIndex, glm::vec3 newPos);
    void UpdateLightRadius(int lightIndex);

    int GetClosestRadiusIndex(int radius);

 
};
