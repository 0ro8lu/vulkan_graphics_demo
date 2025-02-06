#version 450

layout (set = 0, binding = 0) uniform sampler2D samplerColor;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

const float offset = 1.0 / 300.0;

vec2 offsets[9] = vec2[](
    vec2(-offset,  offset), // top-left
    vec2( 0.0f,    offset), // top-center
    vec2( offset,  offset), // top-right
    vec2(-offset,  0.0f),   // center-left
    vec2( 0.0f,    0.0f),   // center-center
    vec2( offset,  0.0f),   // center-right
    vec2(-offset, -offset), // bottom-left
    vec2( 0.0f,   -offset), // bottom-center
    vec2( offset, -offset)  // bottom-right    
);


float hallucination_kernel[9] = float[](
    -1, -1, -1,
    -1,  9, -1,
    -1, -1, -1
);

float none_kernel[9] = float[](
    1, 1, 1,
    1, 1, 1,
    1, 1, 1
);

float blur_kernel[9] = float[](
	1.0 / 16, 2.0 / 16, 1.0 / 16,
    2.0 / 16, 4.0 / 16, 2.0 / 16,
    1.0 / 16, 2.0 / 16, 1.0 / 16
);

vec3 sampleTex[9];

void main()
{
    // HDR stuff
    const float gamma = 2.2;
    vec3 hdrColor = texture(samplerColor, inUV).rgb;
  
    // reinhard tone mapping
    // vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
    vec3 mapped = vec3(1.0) - exp(-hdrColor * 1);
    // gamma correction 
    // mapped = pow(mapped, vec3(1.0 / gamma));
  
    // outColor = vec4(hdrColor, 1.0);
    outColor = vec4(mapped, 1.0);
}
