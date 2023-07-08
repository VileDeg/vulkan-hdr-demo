#include "defs.glsl"

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
    int  enabled;
};

vec3 pointLight(LightData ld, vec3 fragPos, vec3 normal, vec3 cameraPos) {
    if (ld.enabled == 0) {
        return vec3(0);
    }
    
    vec3 lightColor = vec3(ld.color);

    vec3 lightPos   = vec3(ld.pos);
    
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
    vec3 specular = ld.specularFactor * spec * lightColor;  

    // attenuation
    float distance    = length(lightPos - fragPos);
    float attenuation = 1.0 / (ld.constant + ld.linear * distance + 
        ld.quadratic * (distance * distance));    

    vec3 lightVal = diffuse + specular;
    lightVal *= attenuation;
    //lightVal += ambient;
    lightVal *= ld.intensity;

    return lightVal;
}

vec3 calculateLighting(LightData[MAX_LIGHTS] lights, vec3 fragPos, vec3 normal, vec3 cameraPos) {
    vec3 lightVal = vec3(0);
    for (int i = 0; i < MAX_LIGHTS; i++) {
        LightData ld = lights[i];
        /*float dist = distance(ld.pos.xyz, fragPos);
        if (dist > ld.radius) {
    		continue;
	    }*/
		lightVal += pointLight(ld, fragPos, normal, cameraPos.xyz);
	}
    return lightVal;
};