#ifndef _DEFS_GLSL_
#define _DEFS_GLSL_

    #define RGB_TO_LUM vec3(0.2125, 0.7154, 0.0721)

    #define MAX_VIEWPORT_MIPS 13

    #define MAX_LIGHTS 4

    #define MAX_OBJECTS 10
    #define MAX_MESHES_PER_OBJECT 50
    #define MAX_MESHES MAX_OBJECTS * MAX_MESHES_PER_OBJECT

    #define MAX_TEXTURES MAX_MESHES

    #define MAX_LUMINANCE_BINS 256
 

    #define FUSION_WEIGHT_MODE 0
    #define FUSION_BLUR_MODE 2

    #define DURAND_CONVERT_LAB 1

#endif