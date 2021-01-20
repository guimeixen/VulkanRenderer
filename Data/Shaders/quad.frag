#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uv;

layout(set = 1, binding = 0) uniform sampler2D tex;

void main()
{
    outColor = texture(tex, uv);
	outColor.r *= 2.0;
}