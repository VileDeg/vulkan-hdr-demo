#ifndef _DEFS_GLSL_
#define _DEFS_GLSL_

    #define EPSILON 0.0001
    #define RGB_TO_LUM vec3(0.2125, 0.7154, 0.0721)
    #define WHITE_POINT 10.0


    #define MAX_VIEWPORT_MIPS 13


    #define MAX_LIGHTS 4

    #define MAX_OBJECTS 10
    #define MAX_MESHES_PER_OBJECT 50
    #define MAX_MESHES MAX_OBJECTS * MAX_MESHES_PER_OBJECT

    #define MAX_TEXTURES MAX_MESHES

    #define MAX_LUMINANCE_BINS 256
 
    
    #define BLOOM_BLUR_MODE 0

    #define FUSION_WEIGHT_MODE 0
    #define FUSION_BLUR_MODE 2


    // Extract luminance from LAB color space and then use it to restore the color 
    // Benefit: color doesn't get oversaturated when base offset is increased
    // Downside: makes image go grayscale a bit because color remains the same but luminance goes up/down
    // CURRENTLY BROKEN
    #define DURAND_CONVERT_LAB 0

    #define DURAND_LOG_LUM 1
#if DURAND_LOG_LUM == 1
    #define DURAND_LOG_LUM_RES 255 // Luminance will be multiplied by this constant to map 0..1 to a bigger range before taking logarithm of it
#endif
    #define DURAND_APPLY_GTMO 0
    #define DURAND_DETAIL_ABS 0

#endif