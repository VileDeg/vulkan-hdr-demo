#pragma once

#include "types.h"
#include "resources.h"
#include "vk_descriptors.h"

class Engine {
public:
    Engine() : _inp([this]() { drawFrame(); }) {}

    void Init();
    void Run();
    void Cleanup();

private: /* Methods used from Init directly */
    void createWindow(); 
    void createInstance();
    void createDebugMessenger();
    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();
    void createVmaAllocator();
    void createSwapchain();

    void createRenderpass();

    void createSwapchainImages();

    void createViewportImages(uint32_t extentX, uint32_t extentY);

    void createPipelines();
    //void createFramebuffers();
    void createFrameData();
    void createSamplers();

    void createScene(const std::string mainModelFullPath);

private: /* Secondary methods */

    

    void recreateSwapchain();
    void cleanupSwapchainResources();

    void recreateViewport(uint32_t extentX, uint32_t extentY);
    void cleanupViewportResources();

    void initDescriptors();
    void initUploadContext();
    void initFrame(FrameData& f);

    bool loadModelFromObj(const std::string assignedName, const std::string path);

    Texture* loadTextureFromFile(const char* path);
    //Texture* loadTextureFromFileHDR(const char* path);

    void loadCubemap(const char* cubemapDirName, bool isHDR);

    void uploadMesh(Mesh& mesh);
    void createMeshBuffer(Mesh& mesh, bool isVertexBuffer);

    Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

    Material* getMaterial(const std::string& name);
    Mesh* getMesh(const std::string& name);
    Texture* getTexture(const std::string& name);
    Model* getModel(const std::string& name);

    FrameData& getCurrentFrame() { return _frames[_frameInFlightNum]; }

    void drawObject(VkCommandBuffer cmd, const std::shared_ptr<RenderObject>& object, Material** lastMaterial, Mesh** lastMesh, int index);
    void drawObjects(VkCommandBuffer cmd, const std::vector<std::shared_ptr<RenderObject>>& objects);

    void drawFrame();

    AllocatedBuffer createBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    size_t pad_uniform_buffer_size(size_t originalSize);

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

private:  // UI
    void initImgui();

    void imgui_RegisterViewportImageViews();
    void imgui_UnregisterViewportImageViews();

    void imguiUpdate();

    void imguiOnDrawStart();
    void imguiOnRenderPassEnd(VkCommandBuffer cmdBuffer);

    void uiUpdateRenderContext();
    void uiUpdateHDR();

    std::vector<VkDescriptorSet> _imguiViewportImageViewDescriptorSets;

private:
    float _fovY = 90.f; // degrees

    
    
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
    

    Swapchain _swapchain;
    Viewport _viewport;
    
 
    UploadContext _uploadContext;

    VkRenderPass _mainRenderpass;
    VkRenderPass _viewportRenderpass;

    VkPipeline _mainPipeline;

    static constexpr int MAX_FRAMES_IN_FLIGHT = 3;
    FrameData _frames[MAX_FRAMES_IN_FLIGHT];

    VmaAllocator _allocator;

    uint32_t _frameInFlightNum = 0;
    uint32_t _frameNumber = 0;

    float _frameRate = 60.f; // Updated from ImGui io.framerate
    float _deltaTime = 0.016f;
    //bool _framebufferResized = false;

    bool _isInitialized = false;
    DeletionStack _deletionStack{}; // Disposing resources created during initialization
    DeletionStack _sceneDisposeStack{};

    InputContext _inp;
    
    std::vector<std::shared_ptr<RenderObject>> _renderables;

    std::unordered_map<std::string, Model> _models;
    std::unordered_map<std::string, Material> _materials;
    std::unordered_map<std::string, Mesh> _meshes;
    std::unordered_map<std::string, Texture> _textures;

    vkutil::DescriptorAllocator* _descriptorAllocator;
    vkutil::DescriptorLayoutCache* _descriptorLayoutCache;

    VkDescriptorSetLayout _globalSetLayout;
    VkDescriptorSetLayout _objectSetLayout;
    VkDescriptorSetLayout _diffuseTextureSetLayout;
    //VkDescriptorSetLayout _skyboxTextureSetLayout;

    //VkDescriptorSetLayout _compLuminanceSetLayout;

    VkSampler _blockySampler;
    VkSampler _linearSampler;

    VkDescriptorPool _descriptorPool;

    AllocatedBuffer _sceneParameterBuffer;
    AllocatedImage _skyboxAllocImage;

    RenderContext _renderContext;

    GPUData _gpu;

    //Texture* _skyboxTexture;
    std::shared_ptr<RenderObject> _skyboxObject;

private:
    PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;

private:
    const std::vector<const char*> _enabledValidationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    std::vector<const char*> _instanceExtensions{};
    std::vector<const char*> _deviceExtensions{ 
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
        "VK_KHR_maintenance4", "VK_KHR_push_descriptor",
        "VK_EXT_robustness2"
    };

public:
    inline static std::string _assetPath = "assets/";
    inline static std::string shaderPath = _assetPath + "shaders/bin/";
    inline static std::string imagePath  = _assetPath + "images/";
    inline static std::string modelPath  = _assetPath + "models/";

private:
    static constexpr uint32_t WIDTH  = 1280;
    static constexpr uint32_t HEIGHT = 800;

    static constexpr uint32_t FS_WIDTH  = 1920;
    static constexpr uint32_t FS_HEIGHT = 1080;
    static constexpr bool ENABLE_FULLSCREEN = false;
};
