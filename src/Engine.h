#pragma once

#ifdef NDEBUG
#define ENABLE_VALIDATION 0
#define ENABLE_VALIDATION_SYNC 0
#else
#define ENABLE_VALIDATION 1
#define ENABLE_VALIDATION_SYNC 0
#endif

#include "types.h"
#include "camera.h"
#include "vk_descriptors.h"

struct ImGuiInputTextCallbackData;

class Engine {
public:
    Engine();

    void Init();
    void Run();
    void Cleanup();

private: /* Methods used from Init directly */
    void createWindow(); 
    void createInstance();
    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();
    void createVmaAllocator();
    void createSwapchain();

    void prepareMainPass();
    void prepareViewportPass(uint32_t extentX, uint32_t extentY);
    void prepareShadowPass();

    void createPipelines();
    void createFrameData();
    void createSamplers();

    void createScene(CreateSceneData data);

private: /* Secondary methods */

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void cursorCallback(GLFWwindow* window, double xpos, double ypos);
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    void loadInstanceExtensionFunctions();
    void loadDeviceExtensionFunctions();

    void recreateSwapchain();
    void cleanupSwapchainResources();

    void recreateViewport(uint32_t extentX, uint32_t extentY);
    void cleanupViewportResources();

    void initDescriptors();
    void initUploadContext();
    void initFrame(FrameData& f, int frame_i);

    bool loadModelFromObj(const std::string assignedName, const std::string path);


    Attachment* loadTextureFromFile(const char* path);
    void loadCubemap(const char* cubemapDirName, bool isHDR);

    void uploadMesh(Mesh& mesh);
    void createMeshBuffer(Mesh& mesh, bool isVertexBuffer);

    Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);

    void createAttachment(
        VkFormat format, VkImageUsageFlags usage,
        VkExtent3D extent, VkImageAspectFlags aspect,
        VkImageLayout layout,
        Attachment& att,
        const std::string debugName = "");

    void createAttachmentPyramid(
        VkFormat format, VkImageUsageFlags usage,
        VkExtent3D extent, VkImageAspectFlags aspect,
        VkImageLayout layout,
        uint32_t numOfMipLevels,
        AttachmentPyramid& att,
        const std::string debugName = "");

    Material* getMaterial(const std::string& name);
    Mesh* getMesh(const std::string& name);
    Attachment* getTexture(const std::string& name);
    Model* getModel(const std::string& name);

    void drawObject(VkCommandBuffer cmd, const std::shared_ptr<RenderObject>& object, Material** lastMaterial, Mesh** lastMesh, uint32_t index);
    void drawObjects(VkCommandBuffer cmd, const std::vector<std::shared_ptr<RenderObject>>& objects);

    void updateCubeFace(FrameData& f, uint32_t lightIndex, uint32_t faceIndex);

    void loadDataToGPU();
    void recordCommandBuffer(FrameData& f, uint32_t imageIndex);

    void drawFrame();

    AllocatedBuffer allocateBuffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    size_t pad_uniform_buffer_size(size_t originalSize);

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

private:
    void setDisplayLightSourceObjects(bool display);

private:
    void saveScene(std::string fullScenePath);
    void loadScene(std::string fullScenePath);

private:  // UI
    void initImgui();

    void imgui_RegisterViewportImageViews();
    void imgui_UnregisterViewportImageViews();

    void imguiUpdate();

    void imguiOnDrawStart();
    void imguiOnRenderPassEnd(VkCommandBuffer cmdBuffer);

    void uiUpdateScene();
    void uiUpdateHDR();
    void uiUpdateRenderContext();
    void uiUpdateViewport();
    void uiUpdateDebugDisplay();
    void uiUpdateMenuBar();

    bool uiSaveScene();
    bool uiLoadScene();

    bool _saveShortcutPressed = false;
    bool _loadShortcutPressed = false;
    bool _isViewportHovered = true;

    std::vector<VkDescriptorSet> _imguiViewportImageViewDescriptorSets;
private:
    void setDebugName(VkObjectType type, void* handle, const std::string name);

    void beginCmdDebugLabel(VkCommandBuffer cmd, std::string label);
    void beginCmdDebugLabel(VkCommandBuffer cmd, std::string label, glm::vec4 color);

    void endCmdDebugLabel(VkCommandBuffer cmd);

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

    VmaAllocator _allocator;

    FrameData _frames[MAX_FRAMES_IN_FLIGHT];

    UploadContext _uploadContext;

    SwapchainPass _swapchain;
    ViewportPass _viewport;

    ShadowPass _shadow;
    ComputePass _compute;

    uint32_t _frameInFlightNum = 0;

    float _frameRate = 60.f; // Updated from ImGui io.framerate
    float _deltaTime = 0.016f;

    bool _isInitialized = false;
    DeletionStack _deletionStack{}; // Disposing resources created during initialization
    DeletionStack _sceneDisposeStack{};

    Camera _camera = {};

    bool _cursorEnabled = false; 

    std::vector<std::shared_ptr<RenderObject>> _renderables;
    std::shared_ptr<RenderObject> _skyboxObject;

    std::unordered_map<std::string, Model> _models;
    std::unordered_map<std::string, Material> _materials;
    std::unordered_map<std::string, Mesh> _meshes;
    std::unordered_map<std::string, Attachment> _textures;

    vkutil::DescriptorAllocator* _descriptorAllocator;
    vkutil::DescriptorLayoutCache* _descriptorLayoutCache;

    VkDescriptorSetLayout _globalSetLayout;
    VkDescriptorSetLayout _diffuseTextureSetLayout;
    VkDescriptorSetLayout _shadowSetLayout;

    VkSampler _linearSampler;

    VkDescriptorPool _descriptorPool;

    AllocatedImage _skyboxAllocImage;

    Attachment _blurBuffer;

    RenderContext _renderContext;

    GPUData _gpu;

private:
    PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR;

    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;

    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT;

    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;

private:
    std::vector<const char*> _enabledValidationLayers;
    std::vector<const char*> _instanceExtensions;
    std::vector<const char*> _deviceExtensions;

public:
    static std::string ASSET_PATH;
    static std::string SHADER_PATH;
    static std::string IMAGE_PATH;
    static std::string MODEL_PATH;
    static std::string SCENE_PATH;

private:
    uint32_t WIDTH;
    uint32_t HEIGHT;

    uint32_t FS_WIDTH;
    uint32_t FS_HEIGHT;
    bool ENABLE_FULLSCREEN = false;
};
