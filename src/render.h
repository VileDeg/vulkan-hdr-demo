#pragma once

#include "Mesh.h"
#include "types.h"

struct Material {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSet textureSet = { VK_NULL_HANDLE }; //texture defaulted to null

    void cleanup(VkDevice device) {
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
};

struct Texture {
    /* Based on https://github.com/vblanco20-1/vulkan-guide */
    AllocatedImage image;
    VkImageView imageView;
};

struct RenderObject {
    std::string tag = "";
    glm::vec4 color;

    Mesh* mesh;
    Material* material;
    glm::mat4 transform;
};

struct MeshPushConstants {
    glm::ivec4 data;
    glm::mat4 render_matrix;
};

struct GPUCameraData {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
};

#define MAX_LIGHTS 4

struct LightData {
    glm::vec4 color; // r, g, b, a
    glm::vec4 pos;   // x, y, z, radius
    glm::vec4 fac;   // Ambient, diffuse, sepcular, intensity
    glm::vec4 att;   // Constant, linear, quadratic, __unused
};

struct GPUSceneData {
    glm::vec4 cameraPos;
    glm::vec4 ambientColor;
    LightData light[MAX_LIGHTS];

    GPUSceneData() : cameraPos(0.f), ambientColor(0.05f) {
        for (int i = 0; i < MAX_LIGHTS; i++) {
            light[i].color = glm::vec4(1.f);
            light[i].pos = glm::vec4(0.f);
            light[i].fac = glm::vec4(0.5f);
            light[i].att = glm::vec4(0.5f);
        }
    }

    GPUSceneData(glm::vec4 ambCol,
        std::vector<glm::vec3> lightPos,
        std::vector<glm::vec4> lightColor,
        std::vector<float> radius,
        std::vector<float> intensity);
};

struct GPUObjectData {
    glm::mat4 modelMatrix;
    glm::vec4 color = { 1.f, 0.f, 1.f, -1.f }; // magenta
};

struct RenderContext {
    GPUSceneData sceneData;

    std::vector<RenderObject> lightSource;

    std::vector<glm::vec3> lightPos;
    std::vector<glm::vec4> lightColor;

    std::vector<float> radius;
    std::vector<float> intensity;

    void Init();

    void SetCamPos(glm::vec3 pos) {
        sceneData.cameraPos = glm::vec4(pos, 1.f);
    }
};

