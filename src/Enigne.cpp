#include "Enigne.h"

#include <GLFW/glfw3.h>

#include "utils.h"

#include <optional>


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



void Engine::initInstance()
{
    auto requiredExtensions = get_required_extensions();

    const std::vector<const char*> validationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    if (enableValidationLayers && !checkValidationLayerSupport(validationLayers)) {
        throw std::runtime_error("Vulkan: Not all requested validation layers are available!");
    }

    _instance = vk::createInstanceUnique(
        vk::InstanceCreateInfo{
            .pApplicationInfo = &(const vk::ApplicationInfo&)vk::ApplicationInfo{
                .pApplicationName = "Demo",
                .applicationVersion = 1,
                .apiVersion = VK_MAKE_VERSION(1, 0, 0)
            },
            .enabledLayerCount = enableValidationLayers ? (uint32_t)validationLayers.size() : 0,
            .ppEnabledLayerNames = enableValidationLayers ? validationLayers.data() : nullptr,
            .enabledExtensionCount = (uint32_t)requiredExtensions.size(),
            .ppEnabledExtensionNames = requiredExtensions.data()
        }
    );

    /*if (enableValidationLayers) {
        _debugMessanger = _instance->createDebugUtilsMessengerEXTUnique(
            vk::DebugUtilsMessengerCreateInfoEXT{
                .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
                .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
                .pfnUserCallback = debugCallback
            }
        );
    }*/

    
}

static
std::optional<std::vector<std::tuple<vk::PhysicalDevice, uint32_t, uint32_t, vk::PhysicalDeviceProperties>>>
findCompatibleDevices(const vk::Instance& instance, const vk::SurfaceKHR& surface) 
{
    // find compatible devices
    std::vector<vk::PhysicalDevice> deviceList = instance.enumeratePhysicalDevices();
    std::vector<std::tuple<vk::PhysicalDevice, uint32_t, uint32_t, vk::PhysicalDeviceProperties>> compatibleDevices;
    for (vk::PhysicalDevice pd : deviceList) {

        // skip devices without VK_KHR_swapchain
        auto extensionList = pd.enumerateDeviceExtensionProperties();
        for (vk::ExtensionProperties& e : extensionList)
            if (strcmp(e.extensionName, "VK_KHR_swapchain") == 0)
                goto swapchainSupported;
        continue;
    swapchainSupported:

        // select queues for graphics rendering and for presentation
        uint32_t graphicsQueueFamily = UINT32_MAX;
        uint32_t presentationQueueFamily = UINT32_MAX;
        std::vector<vk::QueueFamilyProperties> queueFamilyList = pd.getQueueFamilyProperties();
        for (uint32_t i = 0, c = uint32_t(queueFamilyList.size()); i < c; i++) {

            // test for presentation support
            if (pd.getSurfaceSupportKHR(i, surface)) {

                // test for graphics operations support
                if (queueFamilyList[i].queueFlags & vk::QueueFlagBits::eGraphics) {
                    // if presentation and graphics operations are supported on the same queue,
                    // we will use single queue
                    compatibleDevices.emplace_back(pd, i, i, pd.getProperties());
                    goto nextDevice;
                }
                else
                    // if only presentation is supported, we store the first such queue
                    if (presentationQueueFamily == UINT32_MAX)
                        presentationQueueFamily = i;
            }
            else {
                if (queueFamilyList[i].queueFlags & vk::QueueFlagBits::eGraphics)
                    // if only graphics operations are supported, we store the first such queue
                    if (graphicsQueueFamily == UINT32_MAX)
                        graphicsQueueFamily = i;
            }
        }

        if (graphicsQueueFamily != UINT32_MAX && presentationQueueFamily != UINT32_MAX)
            // presentation and graphics operations are supported on the different queues
            compatibleDevices.emplace_back(pd, graphicsQueueFamily, presentationQueueFamily, pd.getProperties());
    nextDevice:;
    }

    return compatibleDevices;
}

static
std::optional<std::tuple<vk::PhysicalDevice, uint32_t, uint32_t>> pickPhysicalDevice(const vk::Instance& instance, const vk::SurfaceKHR& surface) {
    std::vector<std::tuple<vk::PhysicalDevice, uint32_t, uint32_t, vk::PhysicalDeviceProperties>> 
        compatibleDevices{};
    if (auto result = findCompatibleDevices(instance, surface)) {
        compatibleDevices = result.value();
    } else {
        return std::nullopt;
    }
    
    // print compatible devices
#ifndef NDEBUG
    std::cout << "Compatible devices:" << std::endl;
    for (auto& t : compatibleDevices)
        std::cout << "   " << get<3>(t).deviceName << " (graphics queue: " << get<1>(t)
        << ", presentation queue: " << get<2>(t)
        << ", type: " << to_string(get<3>(t).deviceType) << ")" << std::endl;
#endif // NDEBUG

    // choose the best device
    auto bestDevice = compatibleDevices.begin();
    if (bestDevice == compatibleDevices.end()) {
        return std::nullopt;
    }
        
    constexpr const std::array deviceTypeScore = {
        10, // vk::PhysicalDeviceType::eOther         - lowest score
        40, // vk::PhysicalDeviceType::eIntegratedGpu - high score
        50, // vk::PhysicalDeviceType::eDiscreteGpu   - highest score
        30, // vk::PhysicalDeviceType::eVirtualGpu    - normal score
        20, // vk::PhysicalDeviceType::eCpu           - low score
        10, // unknown vk::PhysicalDeviceType
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
    std::cout << "Using device:\n"
        "   " << get<3>(*bestDevice).deviceName << std::endl;
    vk::PhysicalDevice physicalDevice = get<0>(*bestDevice);
    uint32_t graphicsQueueFamily = get<1>(*bestDevice);
    uint32_t presentationQueueFamily = get<2>(*bestDevice);

    return std::make_tuple(physicalDevice, graphicsQueueFamily, presentationQueueFamily);
}

void Engine::initPhysicalDevice()
{
    VkSurfaceKHR tmp;
    if (glfwCreateWindowSurface(_instance.get(), _window, nullptr, &tmp) != VK_SUCCESS) {
        throw std::runtime_error("GLFW: Failed to create window surface");
    }
    _surface = vk::UniqueSurfaceKHR(tmp, _instance.get());

    if (auto result = pickPhysicalDevice(_instance.get(), _surface.get())) {
        auto&& [pd, gqf, pqf] = result.value();
        _physicalDevice = pd;
        _graphicsQueueFamily = gqf;
        _presentationQueueFamily = pqf;
    } else {
        throw std::runtime_error("No compatible devices found.");
    }
}


void Engine::initWindow()
{
    glfwInit();

    glfwSetErrorCallback(glfw_error_callback);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    if (glfwVulkanSupported == GLFW_FALSE) {
        throw std::runtime_error("GLFW: Vulkan Not Supported");
    }

    _window = glfwCreateWindow(800, 600, "Vulkan", nullptr, nullptr);
}

void Engine::initLogicalDevice()
{
   
    
}
