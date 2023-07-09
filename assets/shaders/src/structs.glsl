struct ObjectData{
	mat4 model;
	vec4 color;

	int useObjectColor;
    int _pad0;
    int _pad1;
    int _pad2;
};

layout(std430, set = 1, binding = 0) buffer GlobalBuffer{
	ObjectData objects[MAX_OBJECTS];
} ssbo;

