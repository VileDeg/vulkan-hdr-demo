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
    bool  enabled;
};

vec3 pointLight(LightData ld, vec3 fragPos, vec3 normal, vec3 cameraPos) {
    
    
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

// array of offset direction for sampling
const vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

float shadowCalculation(samplerCubeArray shadowCubeArray, int lightIndex, vec3 fragPos, vec3 lightPos, vec3 cameraPos, float farPlane, float shadowBias, bool enablePCF)
{
    // Sample shadow cube map
    vec3 lightToFrag = fragPos - lightPos;

	// Check if fragment is in shadow
    float currentDepth = length(lightToFrag);

    float shadow = 0.0;
    
    if (enablePCF) {
        const int PCF_samples = 20;

        float viewDistance = length(cameraPos - fragPos);
        float diskRadius = (1.0 + (viewDistance / farPlane)) / 25.0;
        for(int i = 0; i < PCF_samples; ++i)
        {
            float sampledDepth = texture(shadowCubeArray, vec4(lightToFrag + gridSamplingDisk[i] * diskRadius, lightIndex)).r;
            //sampledDepth *= farPlane;   // undo mapping [0;1]
            if(currentDepth - shadowBias > sampledDepth) {
                shadow += 1.0;
            }
        }
        shadow /= float(PCF_samples);
    } else {
        float sampledDepth = texture(shadowCubeArray, vec4(lightToFrag, lightIndex)).r;
        //sampledDepth *= farPlane;
        if(currentDepth - shadowBias > sampledDepth) {
            shadow = 1.0;
        }
    }

    return shadow;
}

vec3 calculateLighting(LightData[MAX_LIGHTS] lights, vec3 ambientColor, vec3 fragPos, vec3 normal, vec3 cameraPos, 
    bool enableShadows, samplerCubeArray shadowCubeArray, float lightFarPlane, float shadowBias, 
    bool enablePCF) 
{
    vec3 lightVal = ambientColor;
    for (int i = 0; i < MAX_LIGHTS; i++) {
        LightData ld = lights[i];
        if (!ld.enabled) {
            continue;
        }
        /*float dist = distance(ld.pos.xyz, fragPos);
        if (dist > ld.radius) {
    		continue;
	    }*/
        vec3 light = pointLight(ld, fragPos, normal, cameraPos);
        
        // Adjust light intensity for shadow
        if (enableShadows) {
            float shadow = 0.0;
            shadow = shadowCalculation(shadowCubeArray, i, fragPos, ld.pos, cameraPos, lightFarPlane, shadowBias, enablePCF);
            light *= 1.0 - shadow;
        }
        
        lightVal += light;
    }
    return lightVal;
};