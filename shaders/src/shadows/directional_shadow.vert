#version 450

layout (location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(push_constant) uniform UBO {
    mat4 lightSpaceMatrix;
} ubo;

void main()
{
	gl_Position =  ubo.lightSpaceMatrix * vec4(inPos, 1.0);
}
