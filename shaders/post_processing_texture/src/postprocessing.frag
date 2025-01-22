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

float blur_kernel[9] = float[](
	1.0 / 16, 2.0 / 16, 1.0 / 16,
    2.0 / 16, 4.0 / 16, 2.0 / 16,
    1.0 / 16, 2.0 / 16, 1.0 / 16
);

vec3 sampleTex[9];

void main()
{
	//invert
	// outColor = vec4(1.0 - texture(samplerColor, inUV).rgb, 1.0);

	//grayscale
	// vec3 color = texture(samplerColor, inUV).rgb;
	// float average = (0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b) / 3.0;
	// outColor = vec4(average, average, average, 1.0);

	//kernel-based
	for(int i = 0; i < 9; i++)
    {
        sampleTex[i] = vec3(texture(samplerColor, inUV + offsets[i]).rgb);
    }
	vec3 col = vec3(0.0);
    for(int i = 0; i < 9; i++)
        col += sampleTex[i] * blur_kernel[i];
    
    outColor = vec4(col, 1.0);
}
