#include "stdafx.h"
#include "Mesh.h"
#include "Enigne.h"

#include "tinyobj/tiny_obj_loader.h"

void Engine::createVmaAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo = {
        .physicalDevice = _physicalDevice,
        .device = _device,
        .instance = _instance,
    };
    vmaCreateAllocator(&allocatorInfo, &_allocator);

    _deletionStack.push([=]() { vmaDestroyAllocator(_allocator); });
}

bool Mesh::loadFromObj(const std::string& path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    PRINF("Loading model at: " << path);
    bool good = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.data());

    if (!warn.empty()) {
        std::cout << "tinyobj: WARN: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cout << "tinyobj: ERR: " << err << std::endl;
    }

    if (!good || !err.empty()) {
        std::cout << "tinyobj: Failed to load " << path << std::endl;
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



void Engine::loadMeshes()
{
    uint32_t vertexCount = 12;

    _meshes["triangle"] = {};
    Mesh& triMesh = _meshes["triangle"];
    
    triMesh._vertices.resize(vertexCount);

    //back
    triMesh._vertices[0].pos = { 1.f, 1.f, 0.0f };
    triMesh._vertices[1].pos = { -1.f, 1.f, 0.0f };
    triMesh._vertices[2].pos = { 0.f, -1.f, 0.0f };

    float z = -2.f;

    //boottom
    triMesh._vertices[3].pos = { 0.f, 1.f, z };
    triMesh._vertices[4].pos = { -1.f, 1.f, 0.0f };
    triMesh._vertices[5].pos = { 1.f, 1.f, 0.0f };

    //right
    triMesh._vertices[6].pos = { 0.f, 1.f, z };
    triMesh._vertices[7].pos = { 1.f, 1.f, 0.0f };
    triMesh._vertices[8].pos = { 0.f, -1.f, 0.0f };

    //left
    triMesh._vertices[9].pos  = { 0.f, 1.f, z };
    triMesh._vertices[10].pos = { 0.f, -1.f, 0.0f };
    triMesh._vertices[11].pos = { -1.f, 1.f, 0.0f };

    std::vector<glm::vec3> colors = {
        { 1.f, 0.f, 0.f },
        { 0.f, 1.f, 0.f },
        { 0.f, 0.f, 1.f },
    };

    for (uint32_t i = 0; i < vertexCount; ++i) {
        triMesh._vertices[i].color = colors[i % 3];
    }

    _meshes["model"] = {};
    Mesh& modelMesh = _meshes["model"];
    
    modelMesh.loadFromObj(Engine::modelPath + "monkey_smooth.obj");
    //modelMesh.loadFromObj(Engine::modelPath + "holodeck/holodeck.obj");

    triMesh.upload(_allocator);
    modelMesh.upload(_allocator);
}



void Mesh::upload(VmaAllocator allocator)
{
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Vertex) * _vertices.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VmaAllocationCreateInfo allocInfo = {
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU,
    };

    VKASSERT(vmaCreateBuffer(allocator, &bufferInfo, &allocInfo,
        &_vertexBuffer.buffer, &_vertexBuffer.allocation, nullptr));

    void* data;

    vmaMapMemory(allocator, _vertexBuffer.allocation, &data);
    memcpy(data, _vertices.data(), _vertices.size() * sizeof(Vertex));
    vmaUnmapMemory(allocator, _vertexBuffer.allocation);
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