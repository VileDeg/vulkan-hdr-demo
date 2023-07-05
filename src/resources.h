#pragma once

#include "types.h"

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

struct Texture {
    std::string tag = "";
    AllocatedImage image;
    VkImageView imageView;
};

struct Mesh {
    std::string tag = "";

    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;

    std::vector<uint32_t> indices;
    AllocatedBuffer indexBuffer;

    Texture* p_tex{ nullptr };
    int mat_id = -1;

    Material* material{ nullptr };
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
};

struct Light {
    glm::vec3 position{};
    float radius{};

    glm::vec3 color{};
    int _pad0{};

    float ambientFactor{};
    float diffuseFactor{};
    float specularFactor{};
    float intensity{};

    float constant{};
    float linear{};
    float quadratic{};
    int   enabled{ true };
};

struct GPUCameraData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
};

struct GPUPushConstantData {
    int hasTexture;
    int lightAffected;
    int isCubemap;
    int _pad0{ 0 };
};

struct GPUSceneData {
    glm::vec3 cameraPos{};
    int _pad0;

    glm::vec3 ambientColor{};
    int _pad1;

#define MAX_LIGHTS 4
    Light lights[MAX_LIGHTS];
};

struct GPUObjectData {
    glm::mat4 modelMatrix;
    glm::vec4 color = { 1.f, 0.f, 1.f, -1.f }; // magenta

    int useObjectColor;
    int _pad0;
    int _pad1;
    int _pad2;
};

//struct SSBOConfigs {
//    int showNormals{ 0 };
//    float exposure{ 1.0f };
//    int exposureON{ 1 };
//    int _pad0;
//    
//    //int exposureMode{ 0 };
//};

struct GPUSSBOData {
    /*unsigned int newMax{ 0 };
    unsigned int oldMax{ 0 };
    float exposureAverage{ 0 };
    int _pad0;*/

    //SSBOConfigs configs{};
    int showNormals{ 0 };
    float exposure{ 1.0f };
    int enableExposure{ 1 };
    int _pad0;

#define MAX_OBJECTS 10
    GPUObjectData objects[MAX_OBJECTS]{};
};

//struct GPUCompSSBO_ReadOnly {
//
//};

struct GPUCompSSBO {
    float minLogLum;
    float logLumRange;
    float oneOverLogLumRange;
    unsigned int totalPixelNum;

    float averageLuminance = 1.f;
    float targetAverageLuminance = 1.f;
    float timeCoeff = 1.f;
    int _pad0;
    
    unsigned int lumLowerIndex;
    unsigned int lumUpperIndex;
    int _pad1;
    int _pad2;

    glm::vec4 weights;

    int enableToneMapping{ 1 };
    int toneMappingMode{ 3 };
    int enableAdaptation{ 1 };
    int gammaMode{ 0 };

#define MAX_LUMINANCE_BINS 256
    unsigned int luminance[MAX_LUMINANCE_BINS]{};
};

// Struct with pointers to mapped GPU buffer memory
struct GPUData {
    GPUSSBOData* ssbo = nullptr;
    GPUCameraData* camera = nullptr;
    GPUCompSSBO* compLum = nullptr;

    void Reset(FrameData fd);
};

struct RenderContext {
    GPUSceneData sceneData{};
    //GPUSSBOData ssboData{};

    //GPUSceneData* gpu_sd = nullptr;


    //SSBOConfigs ssboConfigs{};

    //GPUPushConstantData pushConstantData{};

    // Using shared ptr because it takes pointers of vector elements which is unsafe if vector gets resized
    std::vector<std::shared_ptr<RenderObject>> lightObjects;

    //glm::vec2 luminanceHistogramBounds{ .3, .95 };

    /*int lumHistStartI = 0;
    int lumHistEndI = MAX_LUMINANCE_BINS-1;*/

    //int totalPixels = 0;

    //float exposureBlendingFactor = 1.2f;
    //float targetAdaptation = 1.f; // Used for plotting

    bool enableSkybox = true;

    void Init();

    void UpdateLightPosition(int lightIndex, glm::vec3 newPos);
    void UpdateLightAttenuation(int lightIndex, int mode /* 0 = Closest */);

    int GetClosestRadiusIndex(int radius);

    std::vector<std::pair<int, glm::vec3>> atten_map{
        // From: https://learnopengl.com/Lighting/Light-casters
        { 7   , {1.0, 0.7   , 1.8     } },
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
