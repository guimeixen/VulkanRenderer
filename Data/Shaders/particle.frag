#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 particleColor;

layout(set = 2, binding = 0) uniform sampler2D tex;

void main()
{
	vec4 tex = texture(tex, uv);
    outColor = tex;
	outColor *= particleColor;
	//outColor.rgb *= outColor.a;
	outColor.a = 0.0;
}