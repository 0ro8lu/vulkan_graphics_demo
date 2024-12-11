#version 450

layout(location = 0) in vec3 inPosition;

layout(binding = 0) uniform StaticUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} sUBO;

layout(binding = 1) uniform DynUBO {
    mat4 model;
} dynUBO;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = sUBO.proj * sUBO.view * dynUBO.model * vec4(inPosition, 1.0);

	fragColor = vec4(1.0);
}
