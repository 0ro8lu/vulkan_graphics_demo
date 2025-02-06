#version 450

layout(set = 1, binding = 0) uniform sampler2D diffuseTexSampler;
layout(set = 1, binding = 1) uniform sampler2D specularTexSampler;

layout(binding = 1) uniform DirectionalLight{
    vec4 direction;
} directionalLight;

struct PointLight {
    vec4 position;
    vec4 color;
};

#define NR_POINT_LIGHTS 5
layout(binding = 2) uniform PointLights {
    PointLight pointLights[NR_POINT_LIGHTS];
} pointLights;

layout(binding = 3) uniform SpotLight{
    vec4 direction;
    vec4 position;
} spotLight;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

float linear = 0.09;
float quadratic = 0.032;

vec3 ambient = vec3(0.2f, 0.2f, 0.2f);
vec3 diffuse = vec3(0.5f, 0.5f, 0.5f);
vec3 specular = vec3(1.0f, 1.0f, 1.0f);

vec3 CalcDirLight(vec3 lightDir, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir);

void main() {

    vec3 norm = normalize(normal);
    vec3 viewDir = normalize(viewPos - fragPos);

    vec3 result = 0.2 * CalcDirLight(vec3(directionalLight.direction.xyz), norm, viewDir);

    for(int i = 0; i < NR_POINT_LIGHTS; i++) {
        result += CalcPointLight(vec3(pointLights.pointLights[i].position.xyz), vec3(pointLights.pointLights[i].color.xyz), norm, fragPos, viewDir);
    }

    outColor = vec4(result, 1.0);
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
    vec3 ambient = ambient * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 diffuse = diffuse * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 specular = specular * spec * vec3(texture(specularTexSampler, fragTexCoord));
    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(vec3 lightPos, vec3 lightColor, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    // lightPos = vec3(lightPos.x, -lightPos.y, lightPos.z);

    vec3 lightDir = normalize(vec3(lightPos.x, -lightPos.y, lightPos.z) - fragPos);

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
    vec3 resultAmbient = ambient * lightColor * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultDiffuse = diffuse * lightColor * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultSpecular = specular * lightColor * spec * vec3(texture(specularTexSampler, fragTexCoord));
    resultAmbient *= attenuation;
    resultDiffuse *= attenuation;
    resultSpecular *= attenuation;
    return (resultAmbient + resultDiffuse + resultSpecular);
}
