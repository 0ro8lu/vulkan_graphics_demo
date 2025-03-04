#version 450

layout (binding = 0) uniform sampler2D samplerColor;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outColor;

void main() 
{
    const float gamma = 2.2;
    vec3 hdrColor = texture(samplerColor, inUV).rgb;
  
    // reinhard tone mapping
    // vec3 mapped = hdrColor / (hdrColor + vec3(1.0));
    // vec3 mapped = vec3(1.0) - exp(-hdrColor * 1);
    // gamma correction 
    // mapped = pow(mapped, vec3(1.0 / gamma));
  
    // outColor = vec4(hdrColor, 1.0);
    // outColor = vec4(mapped, 1.0);

	outColor = texture(samplerColor, inUV);
}
