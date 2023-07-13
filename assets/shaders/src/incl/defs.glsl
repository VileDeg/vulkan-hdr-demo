#ifndef _DEFS_GLSL_
#define _DEFS_GLSL_

    #define MAX_LIGHTS 4

    #define MAX_OBJECTS 10
    #define MAX_MESHES_PER_OBJECT 50

    #define MAX_LUMINANCE_BINS 256

    struct LightData {
        vec3 pos; // x, y, z
        float radius;

        vec3 color; // r, g, b
        float intensity;

        float constant;
        float linear;
        float quadratic;
        bool  enabled;
    };

    struct MatData {
        vec3 ambientColor;
        int _pad0;

        vec3 diffuseColor;
        int _pad1;

        vec3 specularColor; 
        int _pad2;
    };

    struct ObjectData{
	    mat4 model;

        mat4 normalMatrix;
        //vec3 _pad0;
        //vec4 _pad1;

	    //vec4 color;
        MatData mat[MAX_MESHES_PER_OBJECT];
    };

    

#endif