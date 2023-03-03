#pragma once

#include "types.h"
#include "Mesh.h"
#include "Camera.h"
#include "RenderObject.h"

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
    void createVmaAllocator();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    //void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createScene();
    
    void bindPipeline(VkCommandBuffer commandBuffer, VkPipeline pipeline);
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void recreateSwapchain();
    
    
    //void cleanupShaders(PipelineShaders& shaders);
    void cleanupSwapchainResources();
    //void cleanupPipeline();

private:
    void loadMeshes();

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
    //void updateCamera();
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
    VkExtent2D _windowExtent;
    std::vector<VkImageView> _swapchainImageViews;
    std::vector<VkFramebuffer> _swapchainFramebuffers;

    VkImageView _depthImageView;
    AllocatedImage _depthImage;
    VkFormat _depthFormat;
    
    VkRenderPass _renderPass;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
    FrameData _frames[MAX_FRAMES_IN_FLIGHT];
    //FrameData& getCurrentFrame();
    

    VmaAllocator _allocator;

    uint32_t _currentFrame = 0;
    uint32_t _frameNumber = 0;
    bool _framebufferResized = false;

    bool _isInitialized = false;
    DeletionStack _deletionStack{};
private:
    std::vector<RenderObject> _renderables;

    std::unordered_map<std::string, Material> _materials;
    std::unordered_map<std::string, Mesh> _meshes;

    Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

    Material* getMaterial(const std::string& name);

    Mesh* getMesh(const std::string& name);

    void drawObjects(VkCommandBuffer cmd, const std::vector<RenderObject>& objects);
private:
    const std::vector<const char*> _enabledValidationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> _instanceExtensions{};
    std::vector<const char*> _deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
public:
    inline static std::string _assetPath = "assets/";
    inline static std::string shaderPath = _assetPath + "shaders/bin/";
    inline static std::string modelPath  = _assetPath + "models/";
private:
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorCallback (GLFWwindow* window, double xpos, double ypos);

    static constexpr uint32_t WIDTH = 800;
    static constexpr uint32_t HEIGHT = 600;

    static constexpr uint32_t FS_WIDTH = 1920;
    static constexpr uint32_t FS_HEIGHT = 1080;
    static constexpr bool ENABLE_FULLSCREEN = false;

    

#ifdef NDEBUG
    static constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
    static constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif
};
