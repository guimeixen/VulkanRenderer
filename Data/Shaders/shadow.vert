#version 450

layout(location = 0) in vec3 inPos;

layout(set = 0, binding = 0) uniform CameraUBO
{
	mat4 proj;
	mat4 view;
	mat4 model;
	mat4 lightSpaceMatrix;
} ubo;

void main()
{
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPos, 1.0);
}