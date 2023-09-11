#ifndef _COMPUTE_STRUCTS_GLSL_
#define _COMPUTE_STRUCTS_GLSL_

struct ExposureAdaptation {
    float minLogLum;
    float maxLogLum;
    //float logLumRange;
    //float oneOverLogLumRange;
    uint totalPixelNum; // unused
    float timeCoeff;

    uint lumLowerIndex;
    uint lumUpperIndex;
    int _pad0;
    int _pad1;

    vec4 weights; // x - index weight, y - Undefined, z - await. lum. bin, w - awaited lum. weight
};

struct Bloom {
    float threshold;
    float weight;
    float blurRadiusMultiplier;
    int _pad0;
};

struct Durand2002 {
    float baseOffset;
    float baseScale;
    float sigmaS;
    float sigmaR;

    int bilateralRadius;
    int _pad0;
    int _pad1;
    int _pad2;

    vec4 normalizationColor; // unused
};

struct ExposureFusion {
    float shadowsExposure;
    float midtonesExposure;
    float highlightsExposure;
    float exposednessWeightSigma;
};

struct GlobalToneMapping {
    int mode;
    int _pad0;
    int _pad1;
    int _pad2;
};

struct GammaCorrection {
    float gamma;
    int mode; // unused
    int _pad0;
    int _pad1;
};

#endif //_COMPUTE_STRUCTS_GLSL_