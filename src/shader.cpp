#include "stdafx.h"
#include "Enigne.h"

std::vector<char> Engine::readShaderBinary(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    ASSERTMSG(file.is_open(), "Failed to open file: " << filename);

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

bool Engine::createShaderModule(const std::vector<char>& code, VkShaderModule* module) {
    return vkCreateShaderModule(_device, HCCP(VkShaderModuleCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = (uint32_t*)code.data()
    }, nullptr, module) == VK_SUCCESS;
}