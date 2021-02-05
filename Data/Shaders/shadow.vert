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
    gl_Position = projView * GetModelMatrix(startIndex) * vec4(inPos, 1.0);
}