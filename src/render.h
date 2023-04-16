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
    //glm::vec4 color;

    Mesh* mesh;
    Material* material;
    glm::mat4 transform;
};

struct RenderContext {
    RenderObject lightSource;
    glm::vec4 lightPos;
};

