#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 color;
layout(location = 1) out vec2 uv;

layout(binding = 0) uniform CameraUBO
{
	mat4 proj;
	mat4 view;
	mat4 model;
} ubo;

void main()
{
	color = inColor;
	uv = vec2(inUv.x, inUv.y);
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
}