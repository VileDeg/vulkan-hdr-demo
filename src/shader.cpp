#include "stdafx.h"
#include "shader.h"
#include "Engine.h"

std::vector<char> readShaderBinary(const std::string& filename) 
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    ASSERTMSG(file.is_open(), "Failed to open file: " << filename);

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

bool createShaderModule(VkDevice device, const std::vector<char>& code, VkShaderModule* module) 
{
    VkShaderModuleCreateInfo moduleInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = (uint32_t*)code.data()
    };
    
    return vkCreateShaderModule(device, &moduleInfo, nullptr, module) == VK_SUCCESS;
}

