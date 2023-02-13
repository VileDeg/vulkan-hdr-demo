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
    void initPhysicalDevice();
    void initLogicalDevice();

    std::vector<
        std::tuple<VkPhysicalDevice, uint32_t, uint32_t, VkPhysicalDeviceProperties>>
    findCompatibleDevices(const VkInstance& instance, const VkSurfaceKHR& surface);

    std::optional<
        std::tuple<VkPhysicalDevice, uint32_t, uint32_t>>
    pickPhysicalDevice(const VkInstance& instance, const VkSurfaceKHR& surface);

    bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers);

    bool checkInstanceExtensionSupport(const std::vector<const char*>& requiredExtensions);
    std::vector<const char*> get_required_extensions();

private:
    VkInstance _instance;
    VkPhysicalDevice _physicalDevice;
    uint32_t _graphicsQueueFamily;
    uint32_t _presentationQueueFamily;
    VkSurfaceKHR _surface;
    VkDevice _device;
    GLFWwindow* _window;
    VkDebugUtilsMessengerEXT _debugMessenger;
    const std::vector<const char*> enabledValidationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> instanceExtensions{};
    std::vector<const char*> deviceExtensions{};

private:
#ifdef NDEBUG
    static constexpr const bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr const bool ENABLE_VALIDATION_LAYERS = true;
#endif
};
