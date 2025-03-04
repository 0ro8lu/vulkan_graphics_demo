#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoords;

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(push_constant) uniform Model {
    mat4 model;
};

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 texCoord;
layout(location = 2) out vec3 normal;

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;
    texCoord = inTexCoords;
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    normal = normalMatrix * inNormal;

    gl_Position = ubo.proj * ubo.view * worldPos;
}
