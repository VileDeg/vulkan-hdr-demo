#pragma once

#ifdef NDEBUG
#define ENABLE_VALIDATION 0
#define ENABLE_VALIDATION_SYNC 0
#else
#define ENABLE_VALIDATION 1
#define ENABLE_VALIDATION_SYNC 0
#endif

#include "types.h"
#include "postfx.h"

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

    void prepareSwapchainPass();
    void prepareViewportPass(uint32_t extentX, uint32_t extentY);
    void prepareShadowPass();

    void createPipelines();
    void createFrameData();
    void createSamplers();

    void createScene(CreateSceneData data);
    void cleanupScene();

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
    

    void uploadMesh(Mesh& mesh);
    void createMeshBuffer(Mesh& mesh, bool isVertexBuffer);

    void createGraphicsPipeline(
        const std::string& name,
        const std::string vertBinName, const std::string fragBinName,
        VkShaderStageFlags pushConstantsStages, uint32_t pushConstantsSize,
        std::vector<VkDescriptorSetLayout> setLayouts,
        VkFormat colorFormat, VkFormat depthFormat,
        int cullMode);

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

    void updateShadowCubemapFace(FrameData& f, uint32_t lightIndex, uint32_t faceIndex);

    void loadDataToGPU();


    void bloom(FrameData& f, int imageIndex);
    void durand2002(VkCommandBuffer& cmd, int imageIndex);

    void exposureFusion_Downsample(VkCommandBuffer& cmd, int imageIndex, std::string suffix);
    void exposureFusion(VkCommandBuffer& cmd, int imageIndex);

    void shadowPass(FrameData& f, int imageIndex);
    void viewportPass(VkCommandBuffer& cmd, int imageIndex);
    void postfxPass(FrameData& f, int imageIndex);
    void swapchainPass(FrameData& f, int imageIndex);

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

    void loadSkybox(std::string skyboxDirName);
    //void cleanupSkybox();

private:  // UI

    void ui_InitImGui();
    void ui_Init();

    void ui_RegisterTextures();
    void ui_UnregisterTextures();

    void ui_Update();
    void ui_OnDrawStart();
    void ui_OnRenderPassEnd(VkCommandBuffer cmdBuffer);

    void ui_Scene();
    void ui_HDR();
    void ui_RenderContext();
    void ui_Viewport();
    void ui_DebugDisplay();
    void ui_MenuBar();
    void ui_AttachmentViewer();
    void ui_StatusBar();
    void ui_PostFXPipeline();

    bool ui_LoadSkybox();

    bool ui_SaveScene();
    bool ui_LoadScene();

    void ui_Window(std::string name, std::function<void()> func, int flags = 0);
    bool& ui_GetWindowFlag(std::string name);

    std::map<std::string, bool> uiWindows;

    bool _saveShortcutPressed = false;
    bool _loadShortcutPressed = false;
    bool _isViewportHovered = true;

    bool _wasViewportResized = false;

    uint32_t newViewportSizeX = 0;
    uint32_t newViewportSizeY = 0;
private:
    void setDebugName(VkObjectType type, void* handle, const std::string name);

    void beginCmdDebugLabel(VkCommandBuffer cmd, std::string label);
    void beginCmdDebugLabel(VkCommandBuffer cmd, std::string label, glm::vec4 color);

    void endCmdDebugLabel(VkCommandBuffer cmd);

private:
    float _fovY = 90.f; // degrees
    //uint32_t modelLoaderGlobalMeshIndex = 0;
    uint32_t modelLoaderGlobalDiffuseTexIndex = 0;
    uint32_t modelLoaderGlobalBumpTexIndex = 0;

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
    PostFX _postfx;

    uint32_t _frameInFlightNum = 0;

    float _frameRate = 60.f; // Updated from ImGui io.framerate
    float _deltaTime = 0.016f;

    bool _isInitialized = false;
    DeletionStack _deletionStack{}; // Disposing resources created during initialization
    DeletionStack _sceneDisposeStack{};
    DeletionStack _skyboxDisposeStack{};

    Camera _camera = {};

    bool _cursorEnabled = false; 

    std::vector<std::shared_ptr<RenderObject>> _renderables;
    std::shared_ptr<RenderObject> _skyboxObject;

    std::unordered_map<std::string, Model> _models;
    std::unordered_map<std::string, Material> _materials;
    std::unordered_map<std::string, Mesh> _meshes;
    std::unordered_map<std::string, Attachment> _textures;

    std::vector<Attachment*> _diffTexInsertionOrdered;
    std::vector<Attachment*> _bumpTexInsertionOrdered;

    DescriptorAllocator* _descriptorAllocator;
    DescriptorLayoutCache* _descriptorLayoutCache;

    VkDescriptorSetLayout _sceneSetLayout;
    VkDescriptorSetLayout _shadowSetLayout;

    VkSampler _linearSampler;
    VkSampler _nearestSampler;

    VkDescriptorPool _descriptorPool;

    Attachment _skybox;

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

private:
    uint32_t WIDTH;
    uint32_t HEIGHT;

    uint32_t FS_WIDTH;
    uint32_t FS_HEIGHT;
    bool ENABLE_FULLSCREEN = false;
};
