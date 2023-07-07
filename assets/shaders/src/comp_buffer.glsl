layout (std430, set = 0, binding = 0) buffer Luminance {
    float averageLuminance;
    float targetAverageLuminance;
    int _pad0;
    int _pad1;

    uint histogram[MAX_LUMINANCE_BINS];
} ssbo;

layout (std430, set = 0, binding = 1) readonly buffer RO {
    float minLogLum;
    float logLumRange;
    float oneOverLogLumRange;
    uint totalPixelNum;

    float timeCoeff;
    uint lumLowerIndex;
    uint lumUpperIndex;
    int _pad0;

    vec4 weights;

    bool enableToneMapping;
    int toneMappingMode;
    bool enableAdaptation;
    int gammaMode;
} ssbo_ro;

