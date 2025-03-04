#version 450

layout(set = 2, binding = 0) uniform sampler2D position;
layout(set = 2, binding = 1) uniform sampler2D normal;
layout(set = 2, binding = 2) uniform sampler2D albedo;

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

layout(set = 1, binding = 3) uniform SpotLight{
    vec4 direction;
    vec4 position;
} spotLight;

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec3 viewPos;
layout(location = 2) in mat4 lightSpaceMatrix;

layout(location = 0) out vec4 outColor;

float linear = 0.09;
float quadratic = 0.032;

vec3 baseAmbient = vec3(0.2f, 0.2f, 0.2f);
vec3 baseDiffuse = vec3(0.5f, 0.5f, 0.5f);
vec3 baseSpecular = vec3(1.0f, 1.0f, 1.0f);

float CalculateShadow(vec4 fragPosLightSpace); 
vec3 CalcDirLight(vec3 lightDir, vec3 normal, vec3 viewDir, vec3 diffuseColor, float specularIntensity, float shadow);
vec3 CalcPointLight(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, float specularIntensity);

void main() {
 vec3 fragPos = texture(position, texCoord).rgb;
 vec3 normal = texture(normal, texCoord).rgb;
 vec3 diffuse = texture(albedo, texCoord).rgb;
 float specular = texture(albedo, texCoord).a;
 vec3 viewDir = normalize(viewPos - fragPos);

 vec4 inFragPosLightSpace = lightSpaceMatrix * vec4(fragPos, 1.0);
 float shadow = CalculateShadow(inFragPosLightSpace / inFragPosLightSpace.w);

 vec3 result = 0.2 * CalcDirLight(vec3(directionalLight.direction.xyz), normal, viewDir, diffuse, specular, shadow);

 for(int i = 0; i < NR_POINT_LIGHTS; i++) {
     result += CalcPointLight(vec3(pointLights.pointLights[i].position.xyz), vec3(pointLights.pointLights[i].color.xyz), normal, fragPos, viewDir, diffuse, specular);
     // result += CalcPointLight(vec3(pointLights.pointLights[i].position.xyz), vec3(pointLights.pointLights[i].color.xyz), norm, fragPos, viewDir, shadow);
 }

 outColor = vec4(result, 1.0);
}

float CalculateShadow(vec4 fragPosLightSpace)
{
	float shadow = 0.0;
	if (fragPosLightSpace.z > -1.0 && fragPosLightSpace.z < 1.0) {
		float dist = texture(directionalShadowMap, fragPosLightSpace.st).r;
		if (fragPosLightSpace.w > 0.0 && dist < fragPosLightSpace.z) {
			shadow = 1.0;
		}
	}
	return shadow;
}

vec3 CalcDirLight(vec3 lightDir, vec3 normal, vec3 viewDir, vec3 diffuseColor, float specularIntensity, float shadow)
{
    lightDir = normalize(-lightDir);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading phong
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    // blinn-phong 
    // vec3 halfwayDir = normalize(lightDir + viewDir);  
    // float spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);

    // combine results
    vec3 ambient = baseAmbient * diffuseColor;
    vec3 diffuse = baseDiffuse * diff * diffuseColor;
    vec3 specular = baseSpecular * spec * specularIntensity;

    return ambient + ((1.0 - shadow) * (diffuse + specular));
}

// vec3 CalcPointLight(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir, float shadow)
vec3 CalcPointLight(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 diffuseColor, float specularIntensity)
{
    vec3 lightDir = normalize(vec3(lightPos.x, lightPos.y, lightPos.z) - fragPos);

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
    vec3 resultAmbient = baseAmbient * lightColor * diffuseColor;
    vec3 resultDiffuse = baseDiffuse * lightColor * diff * diffuseColor;
    vec3 resultSpecular = baseSpecular * lightColor * spec * specularIntensity;
    resultAmbient *= attenuation;
    resultDiffuse *= attenuation;
    resultSpecular *= attenuation;
    return (resultAmbient + resultDiffuse + resultSpecular);
    // return resultAmbient + ((1.0 - shadow) * (resultDiffuse + resultSpecular));
}
