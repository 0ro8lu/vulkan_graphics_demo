#version 450

layout(set = 2, binding = 0) uniform sampler2D diffuseTexSampler;
layout(set = 2, binding = 1) uniform sampler2D specularTexSampler;

layout(set = 4, binding = 0) uniform sampler2D directionalShadowMap;

layout(set = 1, binding = 1) uniform DirectionalLight{
    vec4 direction;
} directionalLight;

struct PointLight {
    vec4 position;
    vec4 color;
};

#define NR_POINT_LIGHTS 5
layout(set = 1, binding = 2) uniform PointLights {
    PointLight pointLights[NR_POINT_LIGHTS];
} pointLights;

struct SpotLight {
  vec4 position;
  vec4 direction;
  vec4 color;
  vec4 cutoff;  
};

#define NR_SPOT_LIGHTS 5
layout(set = 1, binding = 3) uniform SpotLights {
    SpotLight spotLights[NR_SPOT_LIGHTS];
} spotLights;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 viewPos;
layout(location = 4) in vec4 inFragPosLightSpace;

layout(location = 0) out vec4 outColor;

float linear = 0.09;
float quadratic = 0.032;

vec3 baseAmbient = vec3(0.2f, 0.2f, 0.2f);
vec3 baseDiffuse = vec3(0.5f, 0.5f, 0.5f);
vec3 baseSpecular = vec3(1.0f, 1.0f, 1.0f);

float CalculateShadow(vec4 fragPosLightSpace); 
vec3 CalcDirLight(vec3 lightDir, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLights(vec3 lightPos, vec3 lightDirection, vec3 lightColor, vec2 cutoff, vec3 normal, vec3 fragPos, vec3 viewDir);

void main() {
 vec3 norm = normalize(normal);
 vec3 viewDir = normalize(viewPos - fragPos);

 vec3 result = 0.2 * CalcDirLight(vec3(directionalLight.direction.xyz), norm, viewDir);

 for(int i = 0; i < NR_POINT_LIGHTS; i++) {
     result += CalcPointLight(vec3(pointLights.pointLights[i].position.xyz), vec3(pointLights.pointLights[i].color.xyz), norm, fragPos, viewDir);
     // result += CalcPointLight(vec3(pointLights.pointLights[i].position.xyz), vec3(pointLights.pointLights[i].color.xyz), norm, fragPos, viewDir, shadow);
 }

 for(int i = 0; i < NR_SPOT_LIGHTS; i++) {
    result += CalcSpotLights(vec3(spotLights.spotLights[i].position.xyz), vec3(spotLights.spotLights[i].direction.xyz), vec3(spotLights.spotLights[i].color.xyz), vec2(spotLights.spotLights[i].cutoff.xy), norm, fragPos, viewDir);
 }

 outColor = vec4(result, 1.0);
}

float CalculateShadow(vec4 fragPosLightSpace)
{
	float shadow = 0.0;
    fragPosLightSpace.st = fragPosLightSpace.st * 0.5 + 0.5;
    
	if (fragPosLightSpace.z > -1.0 && fragPosLightSpace.z < 1.0) {
		float dist = texture(directionalShadowMap, fragPosLightSpace.st).r;
		if (fragPosLightSpace.w > 0.0 && dist < fragPosLightSpace.z) {
			shadow = 1.0;
		}
	}
	return shadow;
}

vec3 CalcDirLight(vec3 lightDir, vec3 normal, vec3 viewDir)
{
    lightDir = normalize(-lightDir);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    // phong
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    // blinn-phong 
    // vec3 halfwayDir = normalize(lightDir + viewDir);  
    // float spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);

    // combine results
    vec3 ambient = baseAmbient * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 diffuse = baseDiffuse * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 specular = baseSpecular * spec * vec3(texture(specularTexSampler, fragTexCoord));

    float shadow = CalculateShadow(inFragPosLightSpace / inFragPosLightSpace.w);

    return ambient + ((1.0 - shadow) * (diffuse + specular));
    // return (ambient + diffuse + specular);
}

// vec3 CalcPointLight(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir, float shadow)
vec3 CalcPointLight(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(lightPos - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    // phong
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    // blinn-phong 
    // vec3 halfwayDir = normalize(lightDir + viewDir);  
    // float spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);

    // attenuation
    float distance = length(fragPos - lightPos);

    // float attenuation = 1.0 / (1.0 + linear * distance + quadratic * (distance * distance));    
    float attenuation = 1.0 / (distance * distance);

    // combine results
    vec3 resultAmbient = baseAmbient * lightColor * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultDiffuse = baseDiffuse * lightColor * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultSpecular = baseSpecular * lightColor * spec * vec3(texture(specularTexSampler, fragTexCoord));
    resultAmbient *= attenuation;
    resultDiffuse *= attenuation;
    resultSpecular *= attenuation;
    return (resultAmbient + resultDiffuse + resultSpecular);
    // return resultAmbient + ((1.0 - shadow) * (resultDiffuse + resultSpecular));
}

vec3 CalcSpotLights(vec3 lightPos, vec3 lightDirection, vec3 lightColor, vec2 cutoff, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(lightPos - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading phong
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    // specular blinn-phong 
    // vec3 halfwayDir = normalize(lightDir + viewDir);  
    // float spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);

    // attenuation
    float distance = length(fragPos - lightPos);

    float theta = dot(lightDir, normalize(-lightDirection));
    float epsilon = (cutoff.x - cutoff.y);
    float intensity = clamp((theta - cutoff.y) / epsilon, 0.0, 1.0);
    diff *= intensity;
    spec *= intensity;

    // float attenuation = 1.0 / (1.0 + linear * distance + quadratic * (distance * distance));    
    float attenuation = 1.0 / (distance * distance);

    // combine results
    vec3 resultAmbient = vec3(0);
    // vec3 resultAmbient = baseAmbient * lightColor * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultDiffuse = baseDiffuse * lightColor * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultSpecular = baseSpecular * lightColor * spec * vec3(texture(specularTexSampler, fragTexCoord));
    resultAmbient *= attenuation;
    resultDiffuse *= attenuation;
    resultSpecular *= attenuation;
    return (resultAmbient + resultDiffuse + resultSpecular);
}
