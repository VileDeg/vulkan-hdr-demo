#pragma once

#include "types.h"

struct VertexInputDescription {
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec3 color;

    static VertexInputDescription getDescription();
};;

struct Mesh {
    std::vector<Vertex> vertices;
    AllocatedBuffer vertexBuffer;
};

