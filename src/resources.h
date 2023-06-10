#pragma once

#include "types.h"

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
};

struct Material {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    //bool hasTextures{ true };
    //VkDescriptorSet textureSet = { VK_NULL_HANDLE }; //texture defaulted to null

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
    std::vector<Vertex> _vertices;
    AllocatedBuffer _vertexBuffer;

    Texture* p_tex{ nullptr };
    int mat_id = -1;
    //bool hasTextures = false;

    Material* material{ nullptr };

    size_t getBufferSize() const { return _vertices.size() * sizeof(Vertex); }
};

struct Model {
    std::string tag = "";
    //std::vector<Texture*> textures;
    //std::vector<int> texId; // Meshes' IDs into textures array
    std::vector<Mesh*> meshes;

    bool lightAffected = true;
    
    float maxExtent{ 0.f };
};

struct RenderObject {
    std::string tag = "";
    glm::vec4 color{};

    Model* model;

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
    int _pad0{ 0 };
    int _pad1{ 0 };
};

#define MAX_MESHES_PER_MODEL 5000

//struct GPUMeshData {
//    int hasTextures = 1;
//    int _pad0{ 0 };
//    int _pad1{ 0 };
//    int _pad2{ 0 };
//};

//struct GPUModelData {
//    GPUMeshData meshes[MAX_MESHES_PER_MODEL];
//};

struct GPUObjectData {
    glm::mat4 modelMatrix;
    glm::vec4 color = { 1.f, 0.f, 1.f, -1.f }; // magenta

    /*int lightAffected = 1;
    int  _pad0{ 0 };
    int  _pad1{ 0 };
    int  _pad2{ 0 };*/
    //int modelIndex; // Index into the model array
};

#define MAX_OBJECTS 1500
//#define MAX_MODELS  3

struct GPUSSBOData {
    unsigned int newMax{ 0 };
    unsigned int oldMax{ 0 };
    int showNormals = 0;
    float exposure{ 1.0f };

    int exposureON{ 1 };
    int exposureMode{ 0 };
    int toneMappingON{ 1 };
    int toneMappingMode{ 0 };

    GPUObjectData objects[MAX_OBJECTS];
    //GPUModelData  models[MAX_MODELS];
};

#define MAX_LIGHTS 4

struct GPUSceneData {
    glm::vec3 cameraPos{};
    int _pad0{};

    glm::vec3 ambientColor{};
    int _pad1{};

    Light lights[MAX_LIGHTS];
};

struct RenderContext {
    GPUSceneData sceneData{};
    GPUSSBOData ssboData{};
    GPUPushConstantData pushConstantData{};

    std::vector<std::shared_ptr<RenderObject>> lightObjects;

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
