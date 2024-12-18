#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} ubo;

layout(push_constant) uniform Model {
    mat4 model;
};

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = ubo.proj * ubo.view * model * vec4(inPosition, 1.0);

	fragColor = vec4(1.0);
}
