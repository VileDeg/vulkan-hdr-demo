#pragma once

//#include "stdafx.h"

class Engine
{
public:
    void Init();
    void Draw();
    void Run();
private:
    void initInstance();
    void initPhysicalDevice();
    void initLogicalDevice();
    void initWindow();

private:
    std::optional<
        std::vector<
        std::tuple<vk::PhysicalDevice, uint32_t, uint32_t, vk::PhysicalDeviceProperties>>>
        findCompatibleDevices(const vk::Instance& instance, const vk::SurfaceKHR& surface);

    std::optional<
        std::tuple<vk::PhysicalDevice, uint32_t, uint32_t>>
        pickPhysicalDevice(const vk::Instance& instance, const vk::SurfaceKHR& surface);

    bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers);

    bool checkInstanceExtensionSupport(const std::vector<const char*>& requiredExtensions);

    std::vector<const char*> get_required_extensions();

    

    
private:
    vk::UniqueInstance _instance;
    //vk::UniqueDebugUtilsMessengerEXT _debugMessanger;
    vk::PhysicalDevice _physicalDevice;
    uint32_t _graphicsQueueFamily;
    uint32_t _presentationQueueFamily;

    vk::UniqueSurfaceKHR _surface;
    vk::UniqueDevice _device;

    GLFWwindow* _window;
    
    const std::vector<const char*> enabledValidationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> enabledExtensions{};

private:
#ifdef NDEBUG
    static constexpr const bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr const bool ENABLE_VALIDATION_LAYERS = true;
#endif
};

