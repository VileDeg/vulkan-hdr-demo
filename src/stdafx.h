#pragma once

// Precompiled header.
// Must be included in every .cpp file. 
// Contains commonly used headers. 

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vk_enum_string_helper.h>

#include <vma/vk_mem_alloc.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/hash.hpp>

#include <json/json.hpp>

#include <optional>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <map>
#include <stack>
#include <tuple>
#include <functional>
#include <string>
#include <algorithm>
#include <set>
#include <array>
#include <fstream>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <numeric>

#include "defs.h"

#include "vk_initializers.h"

#include "vk_utils.h"