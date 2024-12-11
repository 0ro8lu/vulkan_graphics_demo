#version 450

layout(binding = 2) uniform sampler2D diffuseTexSampler;
layout(binding = 3) uniform sampler2D specularTexSampler;

layout(binding = 4) uniform DirectionalLight{
    vec4 direction;
} directionalLight;

struct PointLight {
    vec4 position;
};

#define NR_POINT_LIGHTS 4
layout(binding = 5) uniform PointLights {
    PointLight pointLights[NR_POINT_LIGHTS];
} pointLights;

// layout(binding = 5) uniform PointLight{
//     vec4 position;
// } pointLight;

layout(binding = 6) uniform SpotLight{
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
vec3 CalcPointLight(vec3 lightPos, vec3 normal, vec3 fragPos, vec3 viewDir);

void main() {

    vec3 norm = normalize(normal);
    vec3 viewDir = normalize(viewPos - fragPos);

    vec3 result = vec3(0);
    // vec3 result = CalcDirLight(vec3(directionalLight.direction.xyz), norm, viewDir);

    for(int i = 0; i < NR_POINT_LIGHTS; i++) {
        // vec4 lightPos = vec4(0.7f, 0.2f, 2.0f, 1.0f);
        // result += CalcPointLight(vec3(lightPos.xyz), norm, fragPos, viewDir);
        result += CalcPointLight(vec3(pointLights.pointLights[i].position.xyz), norm, fragPos, viewDir);
    }

    // //ambient light
    // vec3 ambient = ambient * texture(diffuseTexSampler, fragTexCoord).rgb;

    // //diffuse light
    // vec3 norm = normalize(normal);
    // vec3 lightDir = normalize(lightPos - fragPos);
    // float diff = max(dot(norm, lightDir), 0.0);
    // vec3 diffuse = diffuse * diff * texture(diffuseTexSampler, fragTexCoord).rgb;
    // // vec3 diffuse = diff * lightColor;

    // //specular
    // float specularStrength = 0.5;
    // vec3 viewDir = normalize(viewPos - fragPos);
    // vec3 reflectDir = reflect(-lightDir, norm);
    // float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    // // vec3 specular = specularStrength * spec * lightColor;
    // vec3 specular = specular * spec * texture(specularTexSampler, fragTexCoord).rgb;

    // vec3 result = ambient + diffuse + specular;
    // // vec3 result = (ambient + diffuse + specular) * objectColor;
    outColor = vec4(result, 1.0);
}

vec3 CalcDirLight(vec3 lightDir, vec3 normal, vec3 viewDir)
{
    lightDir = normalize(-lightDir);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    // combine results
    vec3 ambient = ambient * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 diffuse = diffuse * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 specular = specular * spec * vec3(texture(specularTexSampler, fragTexCoord));
    return (ambient + diffuse + specular);
}

vec3 CalcPointLight(vec3 lightPos, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(lightPos - fragPos);

    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);

    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);

    // attenuation
    float distance = length(lightPos - fragPos);
    float attenuation = 1.0 / (1.0 + linear * distance + quadratic * (distance * distance));    

    // combine results
    vec3 ambient = ambient * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 diffuse = diffuse * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 specular = specular * spec * vec3(texture(specularTexSampler, fragTexCoord));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}
