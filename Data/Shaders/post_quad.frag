#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 uv;

layout(set = 3, binding = 0) uniform sampler2D tex;
layout(set = 2, binding = 2) uniform sampler2D cloudsTexture;

void main()
{
    outColor = texture(tex, uv);
	//outColor.r *= 2.0;
	
	float a = 1.0;
	/*if (linearDepth > 0.99)
	{*/
		vec4 clouds = texture(cloudsTexture, uv);
		outColor.rgb = outColor.rgb * (1.0 - clouds.a) + clouds.rgb;
		a = 1.0 - clouds.a;
	//}
	
	outColor.a = 1.0;
	
	//outColor = texture(cloudsTexture, uv);
}