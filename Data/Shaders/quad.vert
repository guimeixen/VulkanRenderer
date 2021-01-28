#version 450

layout(location = 0) in vec4 inPosUv;

layout(location = 0) out vec2 uv;

void main()
{
	uv = inPosUv.zw;
    gl_Position = vec4(inPosUv.xy * 0.3, 0.0, 1.0);
}