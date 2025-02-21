#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(set = 0, binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(set = 3, binding = 0) uniform LightSpaceMatrix {
    mat4 lightSpaceMatrix;
} lightSpace;

layout(push_constant) uniform Model {
    mat4 model;
};

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 viewPos;
layout(location = 4) out vec4 fragPosLightSpace;

void main() {    
    fragPos = vec3(model * vec4(inPosition, 1.0));

    outNormal = mat3(transpose(inverse(model))) * inNormal;  

    viewPos = vec3(ubo.cameraPos);
    fragTexCoord = inTexCoord;

    fragPosLightSpace = lightSpace.lightSpaceMatrix * model * vec4(inPosition, 1.0);

    gl_Position = ubo.proj * ubo.view * model * vec4(inPosition, 1.0);
}
