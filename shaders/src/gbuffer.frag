#version 450

layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 normal;

layout(set = 1, binding = 0) uniform sampler2D diffuseTexSampler;
layout(set = 1, binding = 1) uniform sampler2D specularTexSampler;

void main()
{    
    gPosition = fragPos;
    
    gNormal = normalize(normal);

    gAlbedoSpec.rgb = texture(diffuseTexSampler, texCoord).rgb;

    gAlbedoSpec.a = texture(specularTexSampler, texCoord).r;
}
