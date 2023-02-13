#pragma once

class Engine {
public:
    void Init();
    void Draw();
    void Run();
    void Cleanup();

private:
    void initWindow();
    void initInstance();
    void initDebugMessenger();
    void initSurface();
    void initPhysicalDevice();
    void initLogicalDevice();
    void initSwapchain();


    void pickSurfaceFormat();

    std::vector<
        std::tuple<VkPhysicalDevice, uint32_t, uint32_t, VkPhysicalDeviceProperties>>
    findCompatibleDevices();

    std::optional<
        std::tuple<VkPhysicalDevice, uint32_t, uint32_t>>
    pickPhysicalDevice();

    bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers);

    bool checkInstanceExtensionSupport(const std::vector<const char*>& requiredExtensions);
    std::vector<const char*> get_required_extensions();

    bool checkDeviceExtensionSupport(VkPhysicalDevice pd);

private:
    VkInstance _instance;
    VkPhysicalDevice _physicalDevice;
    uint32_t _graphicsQueueFamily;
    uint32_t _presentQueueFamily;
    VkSurfaceKHR _surface;
    VkQueue _graphicsQueue;
    VkQueue _presentQueue;
    VkDevice _device;
    VkSurfaceFormatKHR _surfaceFormat;
    
    GLFWwindow* _window;
    VkDebugUtilsMessengerEXT _debugMessenger;



    const std::vector<const char*> _enabledValidationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> _instanceExtensions{};
    std::vector<const char*> _deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

private:
#ifdef NDEBUG
    static constexpr const bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr const bool ENABLE_VALIDATION_LAYERS = true;
#endif
};
