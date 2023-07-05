layout (std430, set = 0, binding = 0) buffer Luminance {
    float minLogLum;
    float logLumRange;
    float oneOverLogLumRange;
    uint totalPixelNum;

    float averageLuminance;
    float targetAverageLuminance;
    float timeCoeff;
    int _pad0;

    uint lumLowerIndex;
    uint lumUpperIndex;
    int _pad1;
    int _pad2;

    vec4 weights;

    int enableToneMapping;
    int toneMappingMode;
    int enableAdaptation;
    int gammaMode;

    uint histogram[MAX_LUMINANCE_BINS];
} ssbo;


