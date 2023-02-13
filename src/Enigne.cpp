#include "stdafx.h"

#include "Enigne.h"
#include "utils.h"

#ifdef NDEBUG
#define BRK abort()
#else
#define BRK __debugbreak()
#endif // !NDEBUG

#define HASSERT(_x, _tru) do{ if ((_x) != _tru) { BRK; } }while(0)
#define HASSERTMSG(_x, _tru, _msg) do{ if ((_x) != _tru) { fprintf(stderr, "Error: %s\n", _msg); BRK; } }while(0)

#define ASSERT(_x) HASSERT(_x, true)
#define ASSERTMSG(_x, _msg) HASSERTMSG(_x, true, _msg)

//Assert macros for functions returning VkResult
#define VKASSERT(_x) HASSERT(_x, VK_SUCCESS)
#define VKASSERTMSG(_x, _msg) HASSERTMSG(_x, VK_SUCCESS, _msg)

//Assert macros for functions returning GLFWbool
#define GLFWASSERTMSG(_x, _msg) HASSERTMSG(_x, GLFW_TRUE, _msg)

//Handy macro for reducing code in aggregate initialization of CreateInfoXXX structs
#define HCCP(_type) &(const _type&)_type

//Macro for dynamic load of extension functions
#define DYNAMIC_LOAD(_varname, _instance, _func)\
        PFN_##_func _varname = (PFN_##_func)vkGetInstanceProcAddr(_instance, #_func);\
        if (_varname == nullptr) { throw std::runtime_error("Failed to load function " #_func); }

void Engine::Init()
{
    initWindow();
    initInstance();
    initPhysicalDevice();
    initLogicalDevice();
}

void Engine::Draw()
{
}

void Engine::Run()
{
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
    }
}

void Engine::initWindow()
{
    glfwInit();

    glfwSetErrorCallback(glfw_error_callback);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWASSERTMSG(glfwVulkanSupported(),
        "GLFW: Vulkan Not Supported");

    _window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
}

void Engine::initInstance()
{
    instanceExtensions = get_required_extensions();

    ASSERTMSG(!ENABLE_VALIDATION_LAYERS || checkValidationLayerSupport(enabledValidationLayers),
        "Not all requested validation layers are available!");
    
    VKASSERT(vkCreateInstance(
        HCCP(VkInstanceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = HCCP(VkApplicationInfo){
                .pApplicationName = "Demo",
                .applicationVersion = 1,
                .apiVersion = VK_MAKE_VERSION(1, 0, 0)
            },
            .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? (uint32_t)enabledValidationLayers.size() : 0,
            .ppEnabledLayerNames = ENABLE_VALIDATION_LAYERS ? enabledValidationLayers.data() : nullptr,
            .enabledExtensionCount = (uint32_t)instanceExtensions.size(),
            .ppEnabledExtensionNames = instanceExtensions.data()
        }, 
        nullptr, &_instance)
    );

    DYNAMIC_LOAD(cdum, _instance, vkCreateDebugUtilsMessengerEXT);

    if (ENABLE_VALIDATION_LAYERS) {
        VKASSERT(cdum(_instance,
            HCCP(VkDebugUtilsMessengerCreateInfoEXT){
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
                .pfnUserCallback = debug_callback
            }, 
            nullptr, &_debugMessenger)
        );
    }
}


void Engine::initPhysicalDevice()
{
    VKASSERTMSG(glfwCreateWindowSurface(_instance, _window, nullptr, &_surface),
        "GLFW: Failed to create window surface");

    deviceExtensions.emplace_back("VK_KHR_swapchain");

    auto result = pickPhysicalDevice(_instance, _surface);
    ASSERTMSG(result.has_value(), "No compatible devices found.");

    auto&& [pd, gqf, pqf] = result.value();
    _physicalDevice = pd;
    _graphicsQueueFamily = gqf;
    _presentationQueueFamily = pqf;
}

void Engine::initLogicalDevice()
{
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {
        VkDeviceQueueCreateInfo{
            .queueFamilyIndex = _graphicsQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &(const float&)1.0f
        },
        VkDeviceQueueCreateInfo{
            .queueFamilyIndex = _presentationQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &(const float&)1.0f
        },
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = _graphicsQueueFamily == _presentationQueueFamily ? uint32_t(1) : static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = ENABLE_VALIDATION_LAYERS ? (uint32_t)enabledValidationLayers.size() : 0,
        .ppEnabledLayerNames = ENABLE_VALIDATION_LAYERS ? enabledValidationLayers.data() : nullptr,
        .enabledExtensionCount = (uint32_t)deviceExtensions.size(),
        .ppEnabledExtensionNames = deviceExtensions.data()
    };

    VKASSERT(vkCreateDevice(_physicalDevice, &deviceCreateInfo, nullptr, &_device));
}


void Engine::Cleanup()
{
    vkDestroyDevice(_device, nullptr);

    if (ENABLE_VALIDATION_LAYERS) {
        DYNAMIC_LOAD(ddum, _instance, vkDestroyDebugUtilsMessengerEXT);
        ddum(_instance, _debugMessenger, nullptr);
    }

    vkDestroySurfaceKHR(_instance, _surface, nullptr);
    vkDestroyInstance(_instance, nullptr);

    glfwDestroyWindow(_window);
    glfwTerminate();
}