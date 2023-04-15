#pragma once

std::vector<char> readShaderBinary(const std::string& filename);

bool createShaderModule(VkDevice device, const std::vector<char>& code, VkShaderModule* module);
