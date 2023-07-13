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

struct Texture {
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
    //AllocatedBuffer matBuffer;

    AllocatedBuffer compSSBO;
    AllocatedBuffer compUB;

    VkDescriptorSet globalSet;
    //VkDescriptorSet objectSet;

    VkDescriptorSet compHistogramSet;
    VkDescriptorSet compAvgLumSet;
    VkDescriptorSet compTonemapSet;

    VkDescriptorSet shadowPassSet;
};

struct ComputeParts {
    VkDescriptorSetLayout setLayout;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
};

struct ComputePass {
    ComputeParts histogram;
    ComputeParts averageLuminance;
    ComputeParts toneMapping;
};

struct ShadowPass {
// Texture properties
    static constexpr int TEX_DIM = 512; //1024
    static constexpr VkFilter TEX_FILTER = VK_FILTER_LINEAR;
// Framebuffer properties
    static constexpr int FB_DIM = TEX_DIM;
    static constexpr VkFormat FB_COLOR_FORMAT = VK_FORMAT_R32_SFLOAT;
    
    uint32_t width, height;

    Texture cubemapArray;
    //std::array<Texture, MAX_LIGHTS> depth;
    Texture depth;

    std::array<std::array<VkImageView  , 6>, MAX_LIGHTS> faceViews;
    std::array<std::array<VkFramebuffer, 6>, MAX_LIGHTS> faceFramebuffers;

    VkSampler sampler;

    VkFormat fbDepthFormat;

    //VkExtent2D imageExtent; // Corresponds to window dimensions

    VkRenderPass renderpass;
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

struct GraphicsPass {
    std::vector<VkImage> images; // Retrieved from created swapchain

    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    VkFormat imageFormat;

    VkImageView depthImageView;
    AllocatedImage depthImage;
    VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;

    VkExtent2D imageExtent; // Corresponds to window dimensions

    VkRenderPass renderpass;

    GraphicsPass() = default;
    GraphicsPass(VkFormat format) : imageFormat{ format } {}
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

    void cleanup(VkDevice device) {
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
};



struct Mesh {
    std::string tag = "";

    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    std::vector<uint32_t> indices;
    AllocatedBuffer indexBuffer;

    Texture* diffuseTex{ nullptr };
    Texture* bumpTex{ nullptr };

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
    //GPUMatUB* mat = nullptr;

    GPUCompSSBO* compSSBO = nullptr;
    GPUCompUB* compUB = nullptr;

    void Reset(FrameData& fd);
};


struct RenderContext {
    GPUSceneUB sceneData{};
    GPUCompUB comp{};

    // Using shared ptr because it takes pointers of vector elements which is unsafe if vector gets resized
    std::vector<std::shared_ptr<RenderObject>> lightObjects;

    bool enableSkybox = true;

    float zNear = 0.1f;
    float zFar = 64.0f;

    //glm::mat4 lightPerspective;
    std::array<glm::mat4, 6> lightView;

    bool showNormals = false;

    float lumPixelLowerBound = 0.2f;
    float lumPixelUpperBound = 0.95f;

    float maxLogLuminance = 10.f;
    float eyeAdaptationTimeCoefficient = 1.1f;

    // Treshold to calculate light's effective radius for optimization
    float lightRadiusTreshold = 1.f / 255.f;

    void Init();

    void UpdateLightPosition(int lightIndex, glm::vec3 newPos);
    //void UpdateLightAttenuation(int lightIndex, int mode /* 0 = Closest */);
    void UpdateLightRadius(int lightIndex);

    int GetClosestRadiusIndex(int radius);

    std::vector<std::pair<int, glm::vec3>> atten_map{
        // From: https://learnopengl.com/Lighting/Light-casters
        { 7, { 1.0, 0.7   , 1.8 } },
        { 13  , {1.0, 0.35  , 0.44    } },
        { 20  , {1.0, 0.22  , 0.20    } },
        { 32  , {1.0, 0.14  , 0.07    } },
        { 50  , {1.0, 0.09  , 0.032   } },
        { 65  , {1.0, 0.07  , 0.017   } },
        { 100 , {1.0, 0.045 , 0.0075  } },
        { 160 , {1.0, 0.027 , 0.0028  } },
        { 200 , {1.0, 0.022 , 0.0019  } },
        { 325 , {1.0, 0.014 , 0.0007  } },
        { 600 , {1.0, 0.007 , 0.0002  } },
        { 3250, {1.0, 0.0014, 0.000007} }
    };
};
