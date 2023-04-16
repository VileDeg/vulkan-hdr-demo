struct Attenuation {
    float constant;
	float linear;
	float quadratic;
};

struct LightParams {
    float ambFactor;
    float diffFactor;
    float specFactor;
};

vec3 calcLighting(vec3 lightColor, vec3 lightPos, vec3 fragPos, vec3 normal, vec3 cameraPos) {
    Attenuation att = Attenuation(1.0, 0.09, 0.032);
    LightParams lp  = LightParams(0.1, 1.0, 0.5);
    
    // ambient
    vec3 ambient = lp.ambFactor * lightColor;
    
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
    vec3 specular = lp.specFactor * spec * lightColor;  

    // attenuation
    float distance    = length(lightPos - fragPos);
    float attenuation = 1.0 / (att.constant + att.linear * distance + 
        att.quadratic * (distance * distance));    

    vec3 lightVal = ambient + diffuse + specular;
    lightVal *= attenuation;

    return lightVal;
}