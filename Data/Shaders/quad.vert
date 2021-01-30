#version 450

layout(location = 0) in vec4 inPosUv;

layout(location = 0) out vec2 uv;

void main()
{
	uv = inPosUv.zw;
    gl_Position = vec4(inPosUv.x * 0.3 - 0.5, inPosUv.y * 0.3 - 0.5, 0.0, 1.0);
}