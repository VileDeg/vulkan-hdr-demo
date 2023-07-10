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
    int hasTexture;
    int lightAffected;
    int isCubemap;
    int _pad0{ 0 };
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
    int _pad0{};

    float ambientFactor{};
    float diffuseFactor{};
    float specularFactor{};
    float intensity{};

    float constant{};
    float linear{};
    float quadratic{};
    GPUBool enabled{ true };
};

struct GPUSceneUB {
    glm::vec3 cameraPos{};
    int _pad0;

    glm::vec3 ambientColor{};
    int _pad1;

    GPUBool showNormals = false;
    float exposure{ 1.0f };
    GPUBool enableExposure = true;
    int _pad2;

    glm::mat4 lightProjMat;

    float lightFarPlane;
    float shadowBias = 0.15f;
    float shadowOpacity; // Unused
    GPUBool showShadowMap = false;

    GPUBool enableShadows = true;
    GPUBool enablePCF = true;
    float shadowMapDisplayBrightness;
    int shadowMapDisplayIndex = 0;

#define MAX_LIGHTS 4
    GPULight lights[MAX_LIGHTS];
};

struct GPUObject {
    glm::mat4 modelMatrix;
    glm::vec4 color = { 1.f, 0.f, 1.f, -1.f }; // magenta

    int useObjectColor;
    int _pad0;
    int _pad1;
    int _pad2;
};

struct GPUSceneSSBO {
#define MAX_OBJECTS 10
    GPUObject objects[MAX_OBJECTS]{};
};


//struct GPUShadowUB {
//    //glm::mat4 model;
//    glm::mat4 projection;
//    glm::vec4 lightPos;
//};

struct GPUCompSSBO {
    float averageLuminance = 1.f;
    float targetAverageLuminance = 1.f;
    int _pad0;
    int _pad1;

#define MAX_LUMINANCE_BINS 256
    unsigned int luminance[MAX_LUMINANCE_BINS]{};
};

struct GPUCompSSBO_ReadOnly {
    float minLogLum = -2.f;
    float logLumRange = 12.f;
    float oneOverLogLumRange;
    unsigned int totalPixelNum;

    float timeCoeff = 1.f;
    unsigned int lumLowerIndex;
    unsigned int lumUpperIndex;
    int _pad0;

    glm::vec4 weights = { 1.f, 128.f, 1.f, 1.f };

    GPUBool enableToneMapping = true;
    int toneMappingMode{ 3 };
    GPUBool enableAdaptation = false; // true
    int gammaMode{ 1 };
};




