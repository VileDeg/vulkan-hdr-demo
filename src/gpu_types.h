#pragma once

#define MAX_LIGHTS 4

#define MAX_OBJECTS 10
#define MAX_MESHES_PER_OBJECT 50
#define MAX_MESHES MAX_OBJECTS * MAX_MESHES_PER_OBJECT
#define MAX_TEXTURES MAX_MESHES

#define MAX_LUMINANCE_BINS 256

// Bool is 8-bit in C++ but 32-bit in GLSL
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
    //GPUBool isCubemap;
    int _pad0;
    uint32_t objectIndex;
    uint32_t meshIndex;

    GPUBool useDiffTex;
    GPUBool useBumpTex;
    uint32_t diffTexIndex;
    uint32_t bumpTexIndex;
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
  
    GPUMaterial mat[MAX_MESHES_PER_OBJECT];
};

struct GPUSceneSSBO {
    GPUObject objects[MAX_OBJECTS]{};
};

struct GPUCompPC {
    int mipIndex;
    GPUBool horizontalPass;
    int _pad0;
    int _pad1;
};

struct GPUCompSSBO {
    float averageLuminance = 1.f;
    float targetAverageLuminance = 1.f;
    int _pad0;
    int _pad1;

    uint32_t luminance[MAX_LUMINANCE_BINS]{};
};




struct ExposureAdaptation {
    float minLogLum = -4.f;
    float maxLogLum = 4.7f;
    //float logLumRange;
    //float oneOverLogLumRange;
    unsigned int totalPixelNum; // unused
    float timeCoeff = 1.f;

    unsigned int lumLowerIndex;
    unsigned int lumUpperIndex;
    int _pad0;
    int _pad1;

    glm::vec4 weights = { 1.f, 1.f, 1.f, 1.f }; // x - index weight, yzw - unused
};

struct Bloom {
    float threshold = 0.5f; // unused
    float weight = 0.03;
    float blurRadiusMultiplier = 0.5f; // unused
    int _pad0;
};

struct Durand2002 {
    float baseOffset = 0;
    float baseScale = 1.0;
    float sigmaS = 20;
    float sigmaR = 0.2;

    int bilateralRadius = 5;
    int _pad0;
    int _pad1;
    int _pad2;

    glm::vec4 normalizationColor = { 1,1,1,1 }; // unused
};

struct ExposureFusion {
    float shadowsExposure = 3.3;
    float midtonesExposure = 0.0f; // unused
    float highlightsExposure = -7;
    float exposednessWeightSigma = 5;
};

struct GlobalToneMapping {
    int mode = 3;
};

struct GammaCorrection {
    float gamma = 2.2f;
    int mode = 0; // unused
};

struct GPUCompUB {
    int numOfViewportMips;
    int _pad0;
    int _pad1;
    int _pad2;

    ExposureAdaptation adp;
    Bloom bloom;
    Durand2002 durand;
    ExposureFusion fusion;
    GlobalToneMapping gtm;
    GammaCorrection gamma;
};
