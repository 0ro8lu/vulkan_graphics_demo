#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform StaticUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} sUBO;

layout(binding = 1) uniform DynUBO {
    mat4 model;
} dynUBO;


layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec3 fragPos;
layout(location = 3) out vec3 viewPos;

void main() {
    gl_Position = sUBO.proj * sUBO.view * dynUBO.model * vec4(inPosition, 1.0);

    outNormal = mat3(transpose(inverse(dynUBO.model))) * inNormal;  
    fragPos = vec3(dynUBO.model * vec4(inPosition, 1.0));
    viewPos = vec3(sUBO.cameraPos);
    fragTexCoord = inTexCoord;
}
