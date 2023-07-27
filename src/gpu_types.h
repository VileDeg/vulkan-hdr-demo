#pragma once

struct GPUBool {
    bool val = {};
    uint8_t _pad[3] = {};

    GPUBool() = default;
    GPUBool(bool v) : val{ v }, _pad{} {}

    operator bool() const { return val; }
    operator bool& () { return val; }

    bool* operator&() { return &val; }
};

struct GPUCameraUB {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
};

struct GPUScenePC {
    GPUBool lightAffected;
    GPUBool isCubemap;
    uint32_t objectIndex;
    uint32_t meshIndex;

    GPUBool useDiffTex;
    GPUBool useBumpTex;
};

struct GPUShadowPC {
    glm::mat4 view;
    float far_plane;
    uint32_t lightIndex;
};

struct GPULight {
    glm::vec3 position{};
    float radius{};

    glm::vec3 color{};
    float intensity{};

    float constant{};
    float linear{};
    float quadratic{};
    GPUBool enabled{ true };
};

struct GPUSceneUB {
    glm::vec3 cameraPos{};
    int _pad0; // 16

    glm::vec3 ambientColor{};
    int _pad1; // 32

    GPUBool showNormals = false;
    float exposure{ 0.0f };
    GPUBool enableExposure = true;
    int _pad2;

    glm::mat4 lightProjMat; 

    float lightFarPlane;
    float shadowBias = 0.15f;
    GPUBool showShadowMap = false;
    int _pad3; 

    GPUBool enableShadows = true;
    GPUBool enablePCF = true;
    float shadowMapDisplayBrightness;
    int shadowMapDisplayIndex = 0;

    GPUBool enableBumpMapping = true;
    float bumpStrength = 2.5f;
    float bumpStep = 0.001f;
    float bumpUVFactor = 0.002f;

#define MAX_LIGHTS 4
    GPULight lights[MAX_LIGHTS];
};

struct GPUMaterial {
    glm::vec3 ambientColor;
    int _pad0;

    glm::vec3 diffuseColor;
    int _pad1;

    glm::vec3 specularColor;
    int _pad2;
};

struct GPUObject {
    glm::mat4 modelMatrix;

    glm::mat4 normalMatrix;
    //glm::vec3 _pad0;
    //glm::vec4 _pad1;

    //glm::vec4 color = { 1.f, 0.f, 1.f, -1.f }; // magenta
#define MAX_MESHES_PER_OBJECT 50
    GPUMaterial mat[MAX_MESHES_PER_OBJECT];
};

struct GPUSceneSSBO {
#define MAX_OBJECTS 10
    GPUObject objects[MAX_OBJECTS]{};
};

struct GPUCompSSBO {
    float averageLuminance = 1.f;
    float targetAverageLuminance = 1.f;
    int _pad0;
    int _pad1;

#define MAX_LUMINANCE_BINS 256
    uint32_t luminance[MAX_LUMINANCE_BINS]{};
};

struct GPUCompUB {
    float minLogLum = -4.f;
    float logLumRange = 6.7f;
    float oneOverLogLumRange;
    uint32_t totalPixelNum;

    float timeCoeff = 1.f;
    uint32_t lumLowerIndex;
    uint32_t lumUpperIndex;
    GPUBool enableLTM = true;

    glm::vec4 weights = { 0.65f, 128.f, 1.f, 1.f }; // x - index w, y - unused, z - awaited lum, w - awaited lum w

    GPUBool enableToneMapping = true;
    int toneMappingMode{ 3 };
    GPUBool enableAdaptation = true;
    int gammaMode{ 1 };
};
