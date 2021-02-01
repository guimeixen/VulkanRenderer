#version 450
#include "ubos.glsl"

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec3 uv;

void main()
{
	uv = inPos;
	mat4 newView = mat4(mat3(ubo.view));		// Remove translation. Pass this in the ubo so we don't do this calculation
    gl_Position = ubo.proj * newView * vec4(inPos, 1.0);
	gl_Position = gl_Position.xyww;
}