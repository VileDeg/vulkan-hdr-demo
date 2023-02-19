#include "stdafx.h"
#include "Mesh.h"
#include "Enigne.h"

void Engine::loadMeshes()
{
    uint32_t vertexCount = 12;
    _triangleMesh.vertices.resize(vertexCount);

    //back
    _triangleMesh.vertices[0].pos = { 1.f, 1.f, 0.0f };
    _triangleMesh.vertices[1].pos = { -1.f, 1.f, 0.0f };
    _triangleMesh.vertices[2].pos = { 0.f, -1.f, 0.0f };

    float z = -2.f;

    //boottom
    _triangleMesh.vertices[3].pos = { 0.f, 1.f, z };
    _triangleMesh.vertices[4].pos = { -1.f, 1.f, 0.0f };
    _triangleMesh.vertices[5].pos = { 1.f, 1.f, 0.0f };

    //right
    _triangleMesh.vertices[6].pos = { 0.f, 1.f, z };
    _triangleMesh.vertices[7].pos = { 1.f, 1.f, 0.0f };
    _triangleMesh.vertices[8].pos = { 0.f, -1.f, 0.0f };

    //left
    _triangleMesh.vertices[9].pos  = { 0.f, 1.f, z };
    _triangleMesh.vertices[10].pos = { 0.f, -1.f, 0.0f };
    _triangleMesh.vertices[11].pos = { -1.f, 1.f, 0.0f };

    std::vector<glm::vec3> colors = {
        { 1.f, 0.f, 0.f },
        { 0.f, 1.f, 0.f },
        { 0.f, 0.f, 1.f },
    };

    for (uint32_t i = 0; i < vertexCount; ++i) {
        _triangleMesh.vertices[i].color = colors[i % 3];
    }


    uploadMesh(_triangleMesh);
}

void Engine::uploadMesh(Mesh& mesh)
{
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Vertex) * mesh.vertices.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };

    VKASSERT(vmaCreateBuffer(_allocator, &bufferInfo, &allocInfo, 
        &mesh.vertexBuffer.buffer, &mesh.vertexBuffer.allocation, nullptr));

    void* data;

    vmaMapMemory(_allocator, mesh.vertexBuffer.allocation, &data);
    memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(_allocator, mesh.vertexBuffer.allocation);

}

VertexInputDescription Vertex::getDescription()
{
    VertexInputDescription description;

    VkVertexInputBindingDescription bindingDescription = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    description.bindings.push_back(bindingDescription);

    VkVertexInputAttributeDescription positionAttribute = {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, pos),
    };

    VkVertexInputAttributeDescription normalAttribute = {
        .location = 1,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, normal),
    };

    VkVertexInputAttributeDescription colorAttribute = {
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, color),
    };

    description.attributes.push_back(positionAttribute);
    description.attributes.push_back(normalAttribute);
    description.attributes.push_back(colorAttribute);

    return description;
}