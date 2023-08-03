#include "stdafx.h"
#include "engine.h"

#define VKDEMO_USE_COMPUTE 1

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    std::cerr << "VALIDATION: " << pCallbackData->pMessage << std::endl << std::endl;

    return VK_FALSE;
}

static bool checkInstanceExtensionSupport(const std::vector<const char*>& requiredExtensions) 
{
    uint32_t propertyCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensionProperties(propertyCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &propertyCount, availableExtensionProperties.data());

#ifndef NDEBUG
    pr("Available extensions: ");
    for (const auto& extensionProperty : availableExtensionProperties) {
        pr("\t" << extensionProperty.extensionName);
    }
    pr("Required extensions: ");
    for (const auto& reqExt : requiredExtensions) {
        pr("\t" << reqExt);
    }
#endif // NDEBUG

    for (const auto& requiredExtension : requiredExtensions) {
        bool extensionFound = false;

        for (const auto& extensionProperty : availableExtensionProperties) {
            if (strcmp(requiredExtension, extensionProperty.extensionName) == 0) {
                extensionFound = true;
                break;
            }
        }

        if (!extensionFound) {
            return false;
        }
    }

    return true;
}

static bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers) 
{
    uint32_t propertyCount = 0;
    vkEnumerateInstanceLayerProperties(&propertyCount, nullptr);
    std::vector<VkLayerProperties> availableLayerProperties(propertyCount);
    vkEnumerateInstanceLayerProperties(&propertyCount, availableLayerProperties.data());

    for (const char* requiredLayerName : requiredLayers) {
        bool layerFound = false;

        for (const auto& layerProperty : availableLayerProperties) {
            if (strcmp(requiredLayerName, layerProperty.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

static std::vector<const char*> get_required_extensions() 
{
    std::vector<const char*> requiredExtensions{};

    uint32_t glfwExtCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    for (uint32_t i = 0; i < glfwExtCount; i++) {
        requiredExtensions.emplace_back(glfwExtensions[i]);
    }

    return requiredExtensions;
}

static bool checkDeviceExtensionSupport(VkPhysicalDevice pd, const std::vector<const char*>& deviceExtensions) 
{
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensionProperties(extensionCount);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &extensionCount, availableExtensionProperties.data());

    bool allSupported = true;
    for (const auto& de : deviceExtensions) {
        bool found = false;
        for (VkExtensionProperties& property : availableExtensionProperties) {
            if (strcmp(property.extensionName, de) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            allSupported = false;
        }
    }

    return allSupported;
}

static std::vector<std::tuple<VkPhysicalDevice, uint32_t, uint32_t, VkPhysicalDeviceProperties>>
findCompatibleDevices(VkInstance instance, VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions) 
{
    // This function is based on the code by https://github.com/pc-john/
    // From his VulkanTutorial series: https://github.com/pc-john/VulkanTutorial/tree/main/05-commandSubmission
    // Repository: https://github.com/pc-john/VulkanTutorial/

    // find compatible devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> deviceList(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

    std::vector<std::tuple<VkPhysicalDevice, uint32_t, uint32_t, VkPhysicalDeviceProperties>> compatibleDevices;
    for (VkPhysicalDevice pd : deviceList) {

        if (!checkDeviceExtensionSupport(pd, deviceExtensions))
            continue;

        VkPhysicalDeviceProperties deviceProperties{};
        vkGetPhysicalDeviceProperties(pd, &deviceProperties);

        // select queues for graphics rendering and for presentation
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, queueFamilyList.data());

        uint32_t graphicsQueueFamily = UINT32_MAX;
        uint32_t presentQueueFamily = UINT32_MAX;
        for (uint32_t i = 0, c = uint32_t(queueFamilyList.size()); i < c; i++) {

            // test for presentation support
            VkBool32 presentationSupported = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &presentationSupported);


            bool allQSupported = queueFamilyList[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
#if VKDEMO_USE_COMPUTE
            allQSupported = allQSupported && (queueFamilyList[i].queueFlags & VK_QUEUE_COMPUTE_BIT);
#endif

            if (presentationSupported) {

                // test for graphics operations support
                if (allQSupported) {
                    // if presentation and graphics operations are supported on the same queue,
                    // we will use single queue

                    compatibleDevices.emplace_back(pd, i, i, deviceProperties);
                    goto nextDevice;
                } else
                    // if only presentation is supported, we store the first such queue
                    if (presentQueueFamily == UINT32_MAX)
                        presentQueueFamily = i;
            } else {
                if (allQSupported)
                    // if only graphics operations are supported, we store the first such queue
                    if (graphicsQueueFamily == UINT32_MAX)
                        graphicsQueueFamily = i;
            }
        }

        if (graphicsQueueFamily != UINT32_MAX && presentQueueFamily != UINT32_MAX)
            // presentation and graphics operations are supported on the different queues
            compatibleDevices.emplace_back(pd, graphicsQueueFamily, presentQueueFamily, deviceProperties);
    nextDevice:;
    }

    return compatibleDevices;
}

static bool checkRequiredDeviceFeaturesSupport(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2& deviceFeatures)
{
    // Check if the device supports the shaderDrawParameters feature
    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);

    auto drFeatures = *reinterpret_cast<VkPhysicalDeviceDynamicRenderingFeatures*>(deviceFeatures.pNext);
    auto shaderDrawParametersFeatures = *reinterpret_cast<VkPhysicalDeviceShaderDrawParametersFeatures*>(drFeatures.pNext);

    bool supported = drFeatures.dynamicRendering &&
        shaderDrawParametersFeatures.shaderDrawParameters;
    return supported;
}


void Engine::loadInstanceExtensionFunctions()
{
    DYNAMIC_LOAD(vkSetDebugUtilsObjectNameEXT, _instance);

    if (ENABLE_VALIDATION) {
        DYNAMIC_LOAD(vkCreateDebugUtilsMessengerEXT, _instance);
        DYNAMIC_LOAD(vkDestroyDebugUtilsMessengerEXT, _instance);
        DYNAMIC_LOAD(vkSetDebugUtilsObjectNameEXT, _instance);
        DYNAMIC_LOAD(vkCmdBeginDebugUtilsLabelEXT, _instance);
        DYNAMIC_LOAD(vkCmdEndDebugUtilsLabelEXT, _instance);
    }
}

void Engine::createInstance()
{
    auto extensions = get_required_extensions();
    for (auto& ext : extensions) {
        _instanceExtensions.push_back(ext);
    }

    if (!checkInstanceExtensionSupport(_instanceExtensions)) {
        throw std::runtime_error("There are unsupported instance extensions.");
    }

    ASSERTMSG(!ENABLE_VALIDATION || checkValidationLayerSupport(_enabledValidationLayers),
        "Not all requested validation layers are available!");

    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Vulkan HDR Demo",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    VkDebugUtilsMessengerCreateInfoEXT dbgMessengerInfo;
    if (ENABLE_VALIDATION) {
        dbgMessengerInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
            .pfnUserCallback = debug_callback
        };
    }

    VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        // Pass debug messenger info to instance info to enable validation for instance creation and destruction
        .pNext = ENABLE_VALIDATION ? &dbgMessengerInfo : nullptr,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = (uint32_t)_enabledValidationLayers.size(),
        .ppEnabledLayerNames = _enabledValidationLayers.data(),
        .enabledExtensionCount = (uint32_t)_instanceExtensions.size(),
        .ppEnabledExtensionNames = _instanceExtensions.data()
    };

    VKASSERT(vkCreateInstance(&instanceInfo, nullptr, &_instance));
    _deletionStack.push([&]() { vkDestroyInstance(_instance, nullptr); });

    loadInstanceExtensionFunctions();

    if (ENABLE_VALIDATION) {
        VKASSERT(vkCreateDebugUtilsMessengerEXT(_instance, &dbgMessengerInfo, nullptr, &_debugMessenger));

        _deletionStack.push([&]() {
            vkDestroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
            });
    }
}

void Engine::createSurface()
{
    VKASSERTMSG(glfwCreateWindowSurface(_instance, _window, nullptr, &_surface),
        "GLFW: Failed to create window surface");

    _deletionStack.push([&]() { vkDestroySurfaceKHR(_instance, _surface, nullptr); });
}

void Engine::pickPhysicalDevice()
{
    // This function is based on the code by https://github.com/pc-john/
    // From his VulkanTutorial series: https://github.com/pc-john/VulkanTutorial/tree/main/05-commandSubmission
    // Repository: https://github.com/pc-john/VulkanTutorial/

    auto compatibleDevices = findCompatibleDevices(_instance, _surface, _deviceExtensions);
    ASSERTMSG(!compatibleDevices.empty(), "No compatible devices found");

    // print compatible devices
#ifndef NDEBUG
    pr("Compatible devices:");
    for (auto& t : compatibleDevices)
        pr("\t" << get<3>(t).deviceName << " (graphics queue: " << get<1>(t)
            << ", presentation queue: " << get<2>(t)
            << ", type: " << std::to_string(get<3>(t).deviceType) << ")");
#endif // NDEBUG

    // choose the best device
    auto bestDevice = compatibleDevices.begin();
    ASSERTMSG(bestDevice != compatibleDevices.end(), "No compatible devices found");

    constexpr const std::array deviceTypeScore = {
        10, // VK_PHYSICAL_DEVICE_TYPE_OTHER          - lowest score
        40, // VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU - high score
        50, // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU   - highest score
        30, // VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU    - normal score
        20, // VK_PHYSICAL_DEVICE_TYPE_CPU            - low score
        10, // unknown VkPhysicalDeviceType
    };
    int bestScore = deviceTypeScore[std::clamp(int(get<3>(*bestDevice).deviceType), 0, int(deviceTypeScore.size()) - 1)];
    if (get<1>(*bestDevice) == get<2>(*bestDevice))
        bestScore++;
    for (auto it = compatibleDevices.begin() + 1; it != compatibleDevices.end(); it++) {
        int score = deviceTypeScore[std::clamp(int(get<3>(*it).deviceType), 0, int(deviceTypeScore.size()) - 1)];
        if (get<1>(*it) == get<2>(*it))
            score++;
        if (score > bestScore) {
            bestDevice = it;
            bestScore = score;
        }
    }
#ifndef NDEBUG
    pr("Using device:\n" << "\t" << get<3>(*bestDevice).deviceName);
#endif // NDEBUG

    _physicalDevice = get<0>(*bestDevice);
    _graphicsQueueFamily = get<1>(*bestDevice);
    _presentQueueFamily = get<2>(*bestDevice);
    _gpuProperties = get<3>(*bestDevice);

    pr("The GPU has a minimum buffer alignment of "
        << _gpuProperties.limits.minUniformBufferOffsetAlignment);
}

void Engine::loadDeviceExtensionFunctions()
{
    DYNAMIC_LOAD(vkCmdPushDescriptorSetKHR, _instance);
}

void Engine::createLogicalDevice()
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{
        VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = _graphicsQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &(const float&)1.0f
        },
        VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = _presentQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &(const float&)1.0f
        },
    };


    // Needed for gl_BaseIndex
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParametersFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES,
        .shaderDrawParameters = VK_TRUE
    };

    // Enable nullDescriptor to pass descriptors with no image view
    VkPhysicalDeviceRobustness2FeaturesEXT robust2features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
        .pNext = &shaderDrawParametersFeatures,
        .nullDescriptor = VK_TRUE
    };

    VkPhysicalDeviceDynamicRenderingFeatures drFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &robust2features,
        .dynamicRendering = VK_TRUE
    };

    VkPhysicalDeviceDescriptorIndexingFeatures diFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .pNext = &drFeatures,
        .descriptorBindingPartiallyBound = true // To allow unused descriptors to remain invalid
    };

    VkPhysicalDeviceFeatures2 deviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &diFeatures
    };

    if (!checkRequiredDeviceFeaturesSupport(_physicalDevice, deviceFeatures)) {
        throw std::runtime_error("Physcial device does not support all required features");
    }

    VkDeviceCreateInfo deviceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &deviceFeatures,
        .queueCreateInfoCount = _graphicsQueueFamily == _presentQueueFamily ? uint32_t(1) : uint32_t(2),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = ENABLE_VALIDATION ? (uint32_t)_enabledValidationLayers.size() : 0,
        .ppEnabledLayerNames = ENABLE_VALIDATION ? _enabledValidationLayers.data() : nullptr,
        .enabledExtensionCount = (uint32_t)_deviceExtensions.size(),
        .ppEnabledExtensionNames = _deviceExtensions.data()
    };

    VKASSERT(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device));

    _deletionStack.push([&]() { vkDestroyDevice(_device, nullptr); });

    vkGetDeviceQueue(_device, _graphicsQueueFamily, 0, &_graphicsQueue);
    vkGetDeviceQueue(_device, _presentQueueFamily, 0, &_presentQueue);

    setDebugName(VK_OBJECT_TYPE_QUEUE, _graphicsQueue, "Main and only queue");

    loadDeviceExtensionFunctions();
}
