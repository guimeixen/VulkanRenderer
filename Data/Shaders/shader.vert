#version 450
#include "ubos.glsl"
#include "utils.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUv;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 color;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec4 lightSpacePos;

layout(push_constant) uniform PushConsts
{
	uint startIndex;
};

void main()
{
	color = inColor;
	uv = vec2(inUv.x, inUv.y);
	
	vec4 wPos = GetModelMatrix(startIndex) * vec4(inPos, 1.0);
	
	lightSpacePos = ubo.lightSpaceMatrix * wPos;
    gl_Position = ubo.proj * ubo.view * wPos;
}