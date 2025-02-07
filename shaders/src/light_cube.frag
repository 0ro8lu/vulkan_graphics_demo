#version 450

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform LightColor {
    layout(offset = 64) vec4 lightColor;
};

void main() {
    outColor = lightColor;
}
