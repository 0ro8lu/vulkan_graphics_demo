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

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec3 viewPos;
layout(location = 2) out mat4 lightSpaceMatrix;
// layout(location = 2) out vec4 fragPosLightSpace;

void main() {    
    viewPos = vec3(ubo.cameraPos);

    lightSpaceMatrix = lightSpace.lightSpaceMatrix;

    // fragPosLightSpace = (biasMat * lightSpace.lightSpaceMatrix * model) * vec4(inPosition, 1.0);

    // gl_Position = vec4(inPosition, 1.0);
    fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(fragTexCoord * 2.0f - 1.0f, 0.0f, 1.0f);
}
