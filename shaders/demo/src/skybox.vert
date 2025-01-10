#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 projection;
    vec4 cameraPos;
} ubo;

layout (location = 0) out vec3 outUVW;

void main() 
{
	outUVW = inPosition;

	// Convert cubemap coordinates into Vulkan coordinate space
	outUVW.xy *= -1.0;

	// Remove translation from view matrix
	mat4 viewMat = mat4(mat3(ubo.view));

	//gl_Position = ubo.projection * viewMat * vec4(inPos.xyz, 1.0);
	gl_Position = ubo.projection * viewMat * vec4(inPosition, 1.0);
    gl_Position.z = gl_Position.w;
}

