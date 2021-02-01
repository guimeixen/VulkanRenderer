layout(std140, set = 0, binding = 0) uniform CameraUBO
{
	mat4 proj;
	mat4 view;
	mat4 projView;
	mat4 lightSpaceMatrix;
} ubo;

layout(std140, set = 0, binding = 1) readonly buffer InstanceDataBuffer
{
	mat4 instanceData[];
};