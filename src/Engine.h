#pragma once

#include "types.h"
#include "Mesh.h"
#include "Camera.h"
#include "render.h"

/**
* Struct that holds data related to immediate command execution.
* This is used for uploading data to the GPU.
*/
struct UploadContext {
    VkFence uploadFence;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
};

struct InputContext {
    InputContext(std::function<void(void)> onFbResize)
        : onFramebufferResize(onFbResize) {}

    Camera camera = {};

    bool cursorEnabled = false;
    bool framebufferResized = false;

    bool toneMappingEnabled = true;
    bool exposureEnabled    = true;

    std::function<void(void)> onFramebufferResize = nullptr;
};

class Engine {
public:
    Engine() : _inp([this]() { drawFrame(); }) {}

    void Init();
    void Run();
    void Cleanup();

private: /* Methods used from Init directly */
    void createWindow(); /**< Init GLFW, create GLFW window, set callbacks. */
    void createInstance();
    void createDebugMessenger();
    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();
    void createVmaAllocator();
    void createSwapchain();
    void createImageViews();
    void createRenderPass();
    void createPipelines();
    void createFramebuffers();
    void createFrameData();

    void loadMeshes();
    void loadTextures();

    void createScene();

private: /* Secondary methods */
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void recreateSwapchain();
    void cleanupSwapchainResources();

    void initDescriptors();
    void initUploadContext();
    void initFrame(FrameData& f);

    void uploadMesh(Mesh& mesh);

    Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
    Material* getMaterial(const std::string& name);
    Mesh* getMesh(const std::string& name);
    FrameData& getCurrentFrame() { return _frames[_frameInFlightNum]; }

    void drawObjects(VkCommandBuffer cmd, const std::vector<RenderObject>& objects);
    void bindPipeline(VkCommandBuffer commandBuffer, VkPipeline pipeline);
    void drawFrame();

    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    size_t pad_uniform_buffer_size(size_t originalSize);

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    bool loadImageFromFile(std::string filePath, AllocatedImage& outImage);

private: 
    void initImgui();
    void imguiCommands();
    void imguiOnDrawStart();
    void imguiOnRenderPassEnd(VkCommandBuffer cmdBuffer);

private:
    int _toneMappingOp = 0; // Reinhard, ACES Narkowicz, ACES Hill
private: 
    GLFWwindow* _window;

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;
    VkSurfaceKHR _surface;

    VkPhysicalDevice _physicalDevice;
    VkPhysicalDeviceProperties _gpuProperties;
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
    
    UploadContext _uploadContext;

    VkRenderPass _renderPass;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
    FrameData _frames[MAX_FRAMES_IN_FLIGHT];

    VmaAllocator _allocator;

    uint32_t _frameInFlightNum = 0;
    uint32_t _frameNumber = 0;
    bool _framebufferResized = false;

    bool _isInitialized = false;
    DeletionStack _deletionStack{};

    InputContext _inp;
    
    std::vector<RenderObject> _renderables;

    std::unordered_map<std::string, Material> _materials;
    std::unordered_map<std::string, Mesh> _meshes;
    std::unordered_map<std::string, Texture> _loadedTextures;

    VkDescriptorSetLayout _globalSetLayout;
    VkDescriptorSetLayout _objectSetLayout;
    VkDescriptorSetLayout _singleTextureSetLayout;

    VkDescriptorPool _descriptorPool;

    //GPUSceneData _sceneParameters;
    AllocatedBuffer _sceneParameterBuffer;
    RenderContext _renderContext;

private:
    const std::vector<const char*> _enabledValidationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> _instanceExtensions{};
    std::vector<const char*> _deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_maintenance4"};
public:
    inline static std::string _assetPath = "assets/";
    inline static std::string shaderPath = _assetPath + "shaders/bin/";
    inline static std::string imagePath  = _assetPath + "models/";
    inline static std::string modelPath  = _assetPath + "models/";
private:
    static constexpr uint32_t WIDTH  = 1280;
    static constexpr uint32_t HEIGHT = 800;

    static constexpr uint32_t FS_WIDTH  = 1920;
    static constexpr uint32_t FS_HEIGHT = 1080;
    static constexpr bool ENABLE_FULLSCREEN = false;
};
