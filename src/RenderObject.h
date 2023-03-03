#pragma once

#include "Mesh.h"

struct Material {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;

    void cleanup(VkDevice device) {
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
};

struct RenderObject {
    Mesh* mesh;
    Material* material;
    glm::mat4 transform;

    
};