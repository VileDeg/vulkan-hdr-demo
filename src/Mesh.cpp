#include "stdafx.h"
#include "Mesh.h"
#include "Engine.h"

#include "tinyobj/tiny_obj_loader.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"

bool Mesh::loadFromObj(const std::string& baseDir, const std::string& objName)
{
    //From https://github.com/tinyobjloader/tinyobjloader
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string path = baseDir + objName;

    PRINF("Loading model at: " << path);
    bool good = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.data(), baseDir.data());

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

            ASSERT(shapes[s].mesh.num_face_vertices[f] == 3);
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
                tinyobj::real_t nx{ 0 }, ny{ 0 }, nz{ 0 };
                if (idx.normal_index >= 0) {
                    nx = attrib.normals[3 * idx.normal_index + 0];
                    ny = attrib.normals[3 * idx.normal_index + 1];
                    nz = attrib.normals[3 * idx.normal_index + 2];
                }
                //vertex uv
                tinyobj::real_t ux{ 0 }, uy{ 0 };
                if (idx.texcoord_index >= 0) {
                    ux = attrib.texcoords[2 * idx.texcoord_index + 0];
                    uy = attrib.texcoords[2 * idx.texcoord_index + 1];
                }

                //copy it into our vertex
                Vertex new_vert{
                    .pos = {vx, vy, vz},
                    .normal = {nx, ny, nz},
                    //we are setting the vertex color as the vertex normal. This is just for display purposess
                    .color = {nx, ny, nz},
                    .uv = {ux, 1-uy}
                };

                _vertices.push_back(new_vert);
            }
            index_offset += fv;

            // per-face material
            shapes[s].mesh.material_ids[f];
        }
    }

    return true;
}

bool Mesh::loadFromGLTF(const std::string& path)
{
    // Load the glTF model using tinygltf
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    bool success = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    if (!warn.empty()) {
        pr("tinygltf: WARN: " << warn);
    }
    if (!err.empty()) {
        pr("tinygltf: ERR: " << err);
    }
    if (!success) {
        pr("tinygltf: Failed to load " << path);
        return false;
    }

    // Extract all the vertices from the model
    if (success) {
        for (const auto& mesh : model.meshes) {
            for (const auto& primitive : mesh.primitives) {
                if (primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                    std::cerr << "Error: only triangle primitives are supported" << std::endl;
                    return 1;
                }

                const auto& position_accessor = model.accessors[primitive.attributes.at("POSITION")];
                const auto& position_buffer_view = model.bufferViews[position_accessor.bufferView];
                const auto& position_buffer = model.buffers[position_buffer_view.buffer];
                const float* position_data_ptr = reinterpret_cast<const float*>(&position_buffer.data[position_accessor.byteOffset + position_buffer_view.byteOffset]);

                if (primitive.attributes.find("NORMAL") == primitive.attributes.end()) {
                    std::cerr << "Error: mesh does not have normals" << std::endl;
                    return 1;
                }
                const auto& normal_accessor = model.accessors[primitive.attributes.at("NORMAL")];
                const auto& normal_buffer_view = model.bufferViews[normal_accessor.bufferView];
                const auto& normal_buffer = model.buffers[normal_buffer_view.buffer];
                const float* normal_data_ptr = reinterpret_cast<const float*>(&normal_buffer.data[normal_accessor.byteOffset + normal_buffer_view.byteOffset]);

                /*const auto& color_accessor = model.accessors[primitive.attributes.at("COLOR_0")];
                const auto& color_buffer_view = model.bufferViews[color_accessor.bufferView];
                const auto& color_buffer = model.buffers[color_buffer_view.buffer];
                const uint8_t* color_data_ptr = &color_buffer.data[color_accessor.byteOffset + color_buffer_view.byteOffset];*/
                const float* uv_data_ptr = nullptr;
                if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end()) {
                    const auto& uv_accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
                    const auto& uv_buffer_view = model.bufferViews[uv_accessor.bufferView];
                    const auto& uv_buffer = model.buffers[uv_buffer_view.buffer];
                    uv_data_ptr = reinterpret_cast<const float*>(&uv_buffer.data[uv_accessor.byteOffset + uv_buffer_view.byteOffset]);
                }

                for (size_t i = 0; i < position_accessor.count; i++) {
                    Vertex vertex;
                    vertex.pos = glm::vec3(position_data_ptr[3 * i], position_data_ptr[3 * i + 1], position_data_ptr[3 * i + 2]);
                    vertex.normal = glm::vec3(normal_data_ptr[3 * i], normal_data_ptr[3 * i + 1], normal_data_ptr[3 * i + 2]);
                    vertex.color = vertex.normal;
                    //vertex.color = glm::vec3(color_data_ptr[4 * i] / 255.0f, color_data_ptr[4 * i + 1] / 255.0f, color_data_ptr[4 * i + 2] / 255.0f);
                    if (uv_data_ptr) {
                        vertex.uv = glm::vec2(uv_data_ptr[2 * i], uv_data_ptr[2 * i + 1]);
                    } else {
                        vertex.uv = glm::vec2(0.0f, 0.0f);
                    }
                    _vertices.push_back(vertex);
                }
            }
        }
    }
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

    VkVertexInputAttributeDescription uvAttribute = {
        .location = 3,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, uv)
    };

    description.attributes.push_back(positionAttribute);
    description.attributes.push_back(normalAttribute);
    description.attributes.push_back(colorAttribute);
    description.attributes.push_back(uvAttribute);

    return description;
}