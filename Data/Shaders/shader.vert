#version 450

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 color;

layout(binding = 0) uniform CameraUBO
{
	mat4 proj;
	mat4 view;
	mat4 model;
} ubo;

void main()
{
	color = inColor;
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 0.0, 1.0);
}