#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 color;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 lightSpacePos;

layout(set = 1, binding = 0) uniform sampler2D shadowMap;
layout(set = 1, binding = 1) uniform sampler2D tex;

void main()
{
	vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
	projCoords.xy = projCoords.xy * 0.5 + 0.5;
	projCoords.y = 1.0 - projCoords.y;		// Flip the y because Vulkan is top left instead of bottom left like OpenGL
	float closestDepth = texture(shadowMap, projCoords.xy).r;
	float currentDepth = projCoords.z;
	float shadow = currentDepth - 0.005 > closestDepth  ? 0.05 : 1.0;		// 0.05 to not make black shadows
	
	if (projCoords.z > 1.0)
		shadow = 1.0;

    outColor = texture(tex, uv) * shadow;
}