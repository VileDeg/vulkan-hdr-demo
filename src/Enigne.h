#pragma once

class Engine {
public:
    void Init();
    void Run();
    void Cleanup();

private:
    void createWindow();
    void createInstance();
    void createDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    
    void drawFrame();
    void recordCommandBuffer();
    void recreateSwapchain();
    void cleanupSwapchain();

    void cleanupPipeline();

    VkShaderModule createShaderModule(const std::vector<char>& code);
    std::vector<char> readShaderBinary(const std::string& filename);

    std::vector<
        std::tuple<VkPhysicalDevice, uint32_t, uint32_t, VkPhysicalDeviceProperties>>
    findCompatibleDevices();

    bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers);

    bool checkInstanceExtensionSupport(const std::vector<const char*>& requiredExtensions);
    std::vector<const char*> get_required_extensions();

    bool checkDeviceExtensionSupport(VkPhysicalDevice pd);

private:
    GLFWwindow* _window;

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;
    VkSurfaceKHR _surface;

    VkPhysicalDevice _physicalDevice;
    VkDevice _device;
    uint32_t _graphicsQueueFamily;
    uint32_t _presentQueueFamily;
    VkQueue _graphicsQueue;
    VkQueue _presentQueue;
    VkSwapchainKHR _swapchain{VK_NULL_HANDLE}; // is passed as .oldSwapchain to vkCreateSwapchainKHR
    std::vector<VkImage> _swapchainImages;
    VkFormat _swapchainImageFormat;
    VkExtent2D _swapchainExtent;
    std::vector<VkImageView> _swapchainImageViews;
    std::vector<VkFramebuffer> _swapchainFramebuffers;
    
    VkRenderPass _renderPass;
    VkPipelineLayout _pipelineLayout;
    VkPipeline _graphicsPipeline;

    VkCommandPool _commandPool;
    std::vector<VkCommandBuffer> _commandBuffers;

    std::vector<VkSemaphore> _imageAvailableSemaphores;
    std::vector<VkSemaphore> _renderFinishedSemaphores;
    std::vector<VkFence> _inFlightFences;

    uint32_t _currentFrame = 0;
    bool _framebufferResized = false;

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    const std::vector<const char*> _enabledValidationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> _instanceExtensions{};
    std::vector<const char*> _deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
private:
    static constexpr const uint32_t WIDTH = 800;
    static constexpr const uint32_t HEIGHT = 600;

    static constexpr const int MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
    static constexpr const bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr const bool ENABLE_VALIDATION_LAYERS = true;
#endif
};
