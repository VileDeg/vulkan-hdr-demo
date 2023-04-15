#include "stdafx.h"
#include "Mesh.h"
#include "Engine.h"

#include "tinyobj/tiny_obj_loader.h"

bool Mesh::loadFromObj(const std::string& path)
{
    //From https://github.com/tinyobjloader/tinyobjloader
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    PRINF("Loading model at: " << path);
    bool good = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.data());

    if (!warn.empty()) {
        pr("tinyobj: WARN: " << warn);
    }
    if (!err.empty()) {
        pr("tinyobj: ERR: " << err);
    }

    if (!good || !err.empty()) {
        pr("tinyobj: Failed to load " << path);
        return false;
    }

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {

            //hardcode loading to triangles
            int fv = 3;

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                // access to vertex
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                //vertex position
                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                //vertex normal
                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

                //copy it into our vertex
                Vertex new_vert;
                new_vert.pos.x = vx;
                new_vert.pos.y = vy;
                new_vert.pos.z = vz;

                new_vert.normal.x = nx;
                new_vert.normal.y = ny;
                new_vert.normal.z = nz;

                //we are setting the vertex color as the vertex normal. This is just for display purposes
                new_vert.color = new_vert.normal;


                _vertices.push_back(new_vert);
            }
            index_offset += fv;
        }
    }

    return true;
}

void Engine::uploadMesh(Mesh& mesh)
{
    mesh.initBuffers(_allocator);

    //allocate vertex buffer
    VkBufferCreateInfo vertexBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        //this is the total size, in bytes, of the buffer we are allocating
        .size = mesh.getBufferSize(),
        //this buffer is going to be used as a Vertex Buffer
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };

    //let the VMA library know that this data should be GPU native
    VmaAllocationCreateInfo vmaAllocInfo = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    //allocate the buffer
    VKASSERT(vmaCreateBuffer(_allocator, &vertexBufferInfo, &vmaAllocInfo,
        &mesh._vertexBuffer.buffer,
        &mesh._vertexBuffer.allocation,
        nullptr));

    immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy copy;
        copy.dstOffset = 0;
        copy.srcOffset = 0;
        copy.size = mesh.getBufferSize();
        vkCmdCopyBuffer(cmd, mesh._stagingBuffer.buffer, mesh._vertexBuffer.buffer, 1, &copy);
        });

    _deletionStack.push([&]() {
        mesh._vertexBuffer.destroy(_allocator);
        });

    // Destroy staging buffer now as we don't need it anymore
    mesh._stagingBuffer.destroy(_allocator);
}

void Mesh::initBuffers(VmaAllocator allocator)
{
    const size_t bufferSize = getBufferSize();

    VkBufferCreateInfo stagingBufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Vertex) * _vertices.size(),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_CPU_ONLY,
    };

    VKASSERT(vmaCreateBuffer(allocator, &stagingBufferInfo, &allocInfo,
        &_stagingBuffer.buffer, &_stagingBuffer.allocation, nullptr));

    void* data;
    vmaMapMemory(allocator, _stagingBuffer.allocation, &data);
    memcpy(data, _vertices.data(), _vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(allocator, _stagingBuffer.allocation);
   
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