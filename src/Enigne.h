#pragma once

#include "types.h"
#include "Mesh.h"
#include "Camera.h"

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
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void recreateSwapchain();
    
    
    void cleanupShaders(PipelineShaders& shaders);
    void cleanupSwapchain();
    void cleanupPipeline();

private:
    void loadMeshes();
    void uploadMesh(Mesh& mesh);

    PipelineShaders loadShaders(const std::string& vertName, const std::string& fragName);
    bool createShaderModule(const std::vector<char>& code, VkShaderModule* module);
    std::vector<char> readShaderBinary(const std::string& filename);

    std::vector<
        std::tuple<VkPhysicalDevice, uint32_t, uint32_t, VkPhysicalDeviceProperties>>
    findCompatibleDevices();

    bool checkValidationLayerSupport(const std::vector<const char*>& requiredLayers);

    bool checkInstanceExtensionSupport(const std::vector<const char*>& requiredExtensions);
    std::vector<const char*> get_required_extensions();

    bool checkDeviceExtensionSupport(VkPhysicalDevice pd);

private:
    Camera _camera{};
    int _fps{};
    float _deltaTime{};
    bool _cursorEnabled{};

    void calculateFPS();
    void calculateDeltaTime();
    void updateCamera();
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
    Mesh _triangleMesh;

    VkCommandPool _commandPool;
    std::vector<VkCommandBuffer> _commandBuffers;

    std::vector<VkSemaphore> _imageAvailableSemaphores;
    std::vector<VkSemaphore> _renderFinishedSemaphores;
    std::vector<VkFence> _inFlightFences;

    VmaAllocator _allocator;

    uint32_t _currentFrame = 0;
    uint32_t _frameNumber = 0;
    bool _framebufferResized = false;

    bool _isInitialized = false;

private:
    const std::vector<const char*> _enabledValidationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> _instanceExtensions{};
    std::vector<const char*> _deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorCallback (GLFWwindow* window, double xpos, double ypos);

    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;

    static constexpr uint32_t FS_WIDTH = 1920;
    static constexpr uint32_t FS_HEIGHT = 1080;
    static constexpr bool ENABLE_FULLSCREEN = false;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;

#ifdef NDEBUG
    static constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif
};
