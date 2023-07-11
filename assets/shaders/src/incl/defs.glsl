#ifndef _DEFS_GLSL_
#define _DEFS_GLSL_

    #define MAX_LIGHTS 4
    #define MAX_OBJECTS 10
    #define MAX_LUMINANCE_BINS 256

    struct LightData {
        vec3 pos; // x, y, z
        float radius;

        vec3 color; // r, g, b
        int  _pad0;

        float ambientFactor;  // TODO: remove
        float diffuseFactor;  // TODO: remove ??
        float specularFactor; 
        float intensity;

        float constant;
        float linear;
        float quadratic;
        bool  enabled;
    };

    struct ObjectData{
	    mat4 model;
	    vec4 color;

	    int useObjectColor;
        int _pad0;
        int _pad1;
        int _pad2;
    };

#endif