#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput inputColor;

layout (location = 0) out vec4 outColor;

vec3 brightnessContrast(vec3 color, float brightness, float contrast) {
	return (color - 0.5) * contrast + 0.5 + brightness;
}

vec3 invert(vec3 color) {
	return vec3(1 -color.x, 1 -color.y, 1 -color.z);
}

vec3 grayscale(vec3 color) {
	float average = (color.x + color.y + color.z) / 3;
	return vec3(average, average, average);
}

void main() 
{
	// Apply brightness and contrast filer to color input
	// Read color from previous color input attachment
	vec3 color = subpassLoad(inputColor).rgb;
	// outColor.rgb = brightnessContrast(color, 1.0f, 2.0f);
	// outColor.rgb = invert(color);
	outColor.rgb = grayscale(color);
}
