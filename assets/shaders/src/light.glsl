#define MAX_LIGHTS 4

struct LightData {
    vec4 color; // r, g, b, a
    vec4 pos; // x, y, z, radius
    vec4 fac; // Ambient, diffuse, sepcular, intensity
    vec4 att; // Constant, linear, quadratic, __unused
};

vec3 pointLight(LightData ld, vec3 fragPos, vec3 normal, vec3 cameraPos) {
    vec3 lightColor = vec3(ld.color);
    float intensity = ld.fac.w;

    vec3 lightPos   = vec3(ld.pos);

    float ambFactor  = ld.fac.x;
    float diffFactor = ld.fac.y;
    float specFactor = ld.fac.z;

    float constant = ld.att.x;
    float linear = ld.att.y;
    float quadratic = ld.att.z;

    // diffuse 
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // specular
    vec3 viewPos = vec3(cameraPos);
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specFactor * spec * lightColor;  

    // attenuation
    float distance    = length(lightPos - fragPos);
    float attenuation = 1.0 / (constant + linear * distance + 
        quadratic * (distance * distance));    

    vec3 lightVal = diffuse + specular;
    lightVal *= attenuation;
    //lightVal += ambient;
    lightVal *= intensity;

    return lightVal;
}



vec3 calculateLighting(LightData[MAX_LIGHTS] lights, vec4 ambientColor, vec3 fragPos, vec3 normal, vec3 cameraPos) {
    vec3 lightVal = ambientColor.rgb;

    for (int i = 0; i < MAX_LIGHTS; i++) {
        LightData ld = lights[i];
        float dist = distance(ld.pos.xyz, fragPos);
        float radius = ld.pos.w;
        //if (dist > radius) {
    	//	continue;
	    //}
		lightVal += pointLight(ld, fragPos, normal, cameraPos.xyz);
	}
    return lightVal;
};