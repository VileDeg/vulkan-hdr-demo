struct ObjectData{
	mat4 model;
	vec4 color;

	int useObjectColor;
    int _pad0;
    int _pad1;
    int _pad2;
};

/*struct Lum {
    int val;
    int _pad0;
    int _pad1;
    int _pad2;
};*/


layout(std430, set = 1, binding = 0) buffer GlobalBuffer{
    uint newMax;
    uint oldMax;
    float exposureAverage;
    int _pad0;
    
    int showNormals;
    float exposure;
    int _pad1;
    int _pad2;

    int exposureON;
    int exposureMode;
    int toneMappingON;
    int toneMappingMode;

	ObjectData objects[MAX_OBJECTS];

    //Lum luminance[MAX_BINS];
    int luminance[MAX_BINS];
} ssbo;

