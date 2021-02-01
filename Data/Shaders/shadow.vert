#version 450
#include "ubos.glsl"
#include "utils.glsl"

layout(location = 0) in vec3 inPos;

layout(push_constant) uniform PushConsts
{
	uint startIndex;
};

void main()
{
    gl_Position = ubo.proj * ubo.view * GetModelMatrix(startIndex) * vec4(inPos, 1.0);
}