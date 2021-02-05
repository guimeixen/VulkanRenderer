#version 450
#include "ubos.glsl"

layout(location = 0) in vec4 posuv;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 camRay;

vec3 UVToCamRay(vec2 uv)
{
	mat4 m = cloudsInvProjJitter;
	m[1][1] *= -1.0;		// Flip the sign of the y because in Vulkan clip space the y start on the top
	m[3][1] *= -1.0;		// Flip the sign of the y translation in the jittered projection matrix

	vec4 clipSpacePos = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
	vec4 viewSpacePos = m * clipSpacePos;
	viewSpacePos.xyz /= viewSpacePos.w;
	vec4 worldSpacePos = invView * viewSpacePos;
	
	return worldSpacePos.xyz;
}

void main()
{
	uv = vec2(posuv.z, posuv.w);
	camRay = UVToCamRay(uv);
	gl_Position = vec4(posuv.x, posuv.y, 0.1, 1.0);
}