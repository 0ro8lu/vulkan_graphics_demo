#version 450

layout(set = 2, binding = 0) uniform sampler2D diffuseTexSampler;
layout(set = 2, binding = 1) uniform sampler2D specularTexSampler;

layout(set = 3, binding = 0) uniform sampler2D directionalShadowMap;
layout(set = 3, binding = 1) uniform sampler2D spotPointShadowAtlas;

layout(set = 1, binding = 1) uniform DirectionalLight{
    vec4 direction;
    vec4 color;
    mat4 transform;
} directionalLight;

struct PointLight {
    vec4 position;
    vec4 color;
    mat4 transform[6];
    vec4 atlasCoordsPixel[6];
    vec4 atlasCoordsNormalized[6];
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
  mat4 transform;
  vec4 atlasCoordsPixel;
  vec4 atlasCoordsNormalized;
};

#define NR_SPOT_LIGHTS 2
layout(set = 1, binding = 3) uniform SpotLights {
    SpotLight spotLights[NR_SPOT_LIGHTS];
} spotLights;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 fragPos;
layout(location = 3) in vec3 viewPos;

layout(location = 0) out vec4 outColor;

float linear = 0.09;
float quadratic = 0.032;

vec3 baseAmbient = vec3(0.2f, 0.2f, 0.2f);
vec3 baseDiffuse = vec3(0.5f, 0.5f, 0.5f);
vec3 baseSpecular = vec3(1.0f, 1.0f, 1.0f);

float CalculateShadow(vec4 fragPosLightSpace); 
float CalculateShadow(vec4 fragPosLightSpavce, vec4 atlasCoords);
vec3 CalcDirLight(vec3 lightDir, vec4 color, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight pointLight, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLights(vec3 lightPos, vec3 lightDirection, vec4 lightColor, vec2 cutoff, vec3 normal, vec3 fragPos, vec3 viewDir, vec4 altasCoords, mat4 lightTransform);

void main() {
 vec3 norm = normalize(normal);
 vec3 viewDir = normalize(viewPos - fragPos);

 vec3 result = 0.2 * CalcDirLight(vec3(directionalLight.direction.xyz), directionalLight.color, norm, viewDir);

 for(int i = 0; i < NR_POINT_LIGHTS; i++) {
     result += CalcPointLight(pointLights.pointLights[i], norm, fragPos, viewDir);
 }

 for(int i = 0; i < NR_SPOT_LIGHTS; i++) {
    result += CalcSpotLights(vec3(spotLights.spotLights[i].position.xyz), vec3(spotLights.spotLights[i].direction.xyz), spotLights.spotLights[i].color, /*vec3(spotLights.spotLights[i].color.xyz),*/ vec2(spotLights.spotLights[i].cutoff.xy), norm, fragPos, viewDir, spotLights.spotLights[i].atlasCoordsNormalized, spotLights.spotLights[i].transform);
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

float CalculateShadow(vec4 fragPosLightSpace, vec4 atlasCoords)
{    
    fragPosLightSpace.st = fragPosLightSpace.st * 0.5 + 0.5;

    if (fragPosLightSpace.z < -1.0 || fragPosLightSpace.z > 1.0 ||
        fragPosLightSpace.x < -1.0 || fragPosLightSpace.x > 1.0 ||
        fragPosLightSpace.y < -1.0 || fragPosLightSpace.y > 1.0) {
        return 0.0;
    }
        
    vec2 atlasUV = atlasCoords.xy + fragPosLightSpace.xy * atlasCoords.zw;
    
    float closestDepth = texture(spotPointShadowAtlas, atlasUV).r;
    float currentDepth = fragPosLightSpace.z;
    
    float shadow = (currentDepth) > closestDepth ? 1.0 : 0.0;
    
    return shadow;
}

vec3 CalcDirLight(vec3 lightDir, vec4 color, vec3 normal, vec3 viewDir)
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
    vec3 ambient = baseAmbient * color.xyz * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 diffuse = baseDiffuse * color.xyz * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 specular = baseSpecular * color.xyz * spec * vec3(texture(specularTexSampler, fragTexCoord));

    vec4 fragPosLightSpace = directionalLight.transform * vec4(fragPos, 1.0);

    float shadow = 0;

    if(color.w == 1.0) {
        shadow = CalculateShadow(fragPosLightSpace / fragPosLightSpace.w);
    }

    return ambient + ((1.0 - shadow) * (diffuse + specular));
    // return (ambient + diffuse + specular);
}

vec3 CalcPointLight(PointLight pointLight, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(pointLight.position.xyz - fragPos);

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
    float distance = length(fragPos - pointLight.position.xyz);

    // float attenuation = 1.0 / (1.0 + linear * distance + quadratic * (distance * distance));    
    float attenuation = 1.0 / (distance * distance);

    vec3 fragToLight = fragPos - pointLight.position.xyz;
    vec3 absFragToLight = abs(fragToLight);
    
    float maxComponent = max(absFragToLight.x, max(absFragToLight.y, absFragToLight.z));

    int faceIndex = 0;
    if (maxComponent == absFragToLight.x) {
        faceIndex = (fragToLight.x > 0.0) ? 3 : 2; // RIGHT=3, LEFT=2
    } else if (maxComponent == absFragToLight.y) {
        faceIndex = (fragToLight.y > 0.0) ? 0 : 1; // UP=0, DOWN=1
    } else {
        faceIndex = (fragToLight.z > 0.0) ? 4 : 5; // FORWARD=4, BACK=5
    }

    vec4 fragPosLightSpace = pointLight.transform[faceIndex] * vec4(fragPos, 1.0);

    float shadow = 0;
    if(pointLight.color.w == 1.0) {
        shadow = CalculateShadow(fragPosLightSpace / fragPosLightSpace.w, pointLight.atlasCoordsNormalized[faceIndex]);
    }

    // combine results
    // vec3 resultAmbient = baseAmbient * pointLight.color.xyz * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultDiffuse = baseDiffuse * pointLight.color.xyz * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultSpecular = baseSpecular * pointLight.color.xyz * spec * vec3(texture(specularTexSampler, fragTexCoord));
    // resultAmbient *= attenuation;
    vec3 resultAmbient = vec3(0);
    resultDiffuse *= attenuation;
    resultSpecular *= attenuation;
    // return (resultAmbient + resultDiffuse + resultSpecular);
    return resultAmbient + ((1.0 - shadow) * (resultDiffuse + resultSpecular));
}

vec3 CalcSpotLights(vec3 lightPos, vec3 lightDirection, vec4 lightColor, vec2 cutoff, vec3 normal, vec3 fragPos, vec3 viewDir, vec4 atlasCoords, mat4 lightTransform)
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

    float innerCutoff = cos(radians(cutoff.x));
    float outerCutoff = cos(radians(cutoff.y));

    float theta = dot(lightDir, normalize(-lightDirection));
    float epsilon = (innerCutoff - outerCutoff);
    float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
    diff *= intensity;
    spec *= intensity;

    // float attenuation = 1.0 / (1.0 + linear * distance + quadratic * (distance * distance));    
    float attenuation = 1.0 / (distance * distance);

    vec4 fragPosLightSpace = lightTransform * vec4(fragPos, 1.0);

    float shadow = 0;
    if(lightColor.w == 1.0) {
        shadow = CalculateShadow(fragPosLightSpace / fragPosLightSpace.w, atlasCoords);
    }

    // combine results
    vec3 resultAmbient = vec3(0);
    // vec3 resultAmbient = baseAmbient * lightColor.xyz * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultDiffuse = baseDiffuse * lightColor.xyz * diff * vec3(texture(diffuseTexSampler, fragTexCoord));
    vec3 resultSpecular = baseSpecular * lightColor.xyz * spec * vec3(texture(specularTexSampler, fragTexCoord));
    // resultAmbient *= attenuation;
    resultDiffuse *= attenuation;
    resultSpecular *= attenuation;
    return resultAmbient + ((1.0 - shadow) * (resultDiffuse + resultSpecular));
}
