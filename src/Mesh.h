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

struct Mesh {
    std::vector<Vertex> _vertices;
    AllocatedBuffer _stagingBuffer;
    AllocatedBuffer _vertexBuffer;

    bool loadFromObj(const std::string& baseDir, const std::string& objName);
    bool loadFromGLTF(const std::string& path);
    void initBuffers(VmaAllocator allocator);
    size_t getBufferSize() const { return _vertices.size() * sizeof(Vertex); }

    void cleanup(VmaAllocator allocator) {
        _stagingBuffer.destroy(allocator);
    }
};

