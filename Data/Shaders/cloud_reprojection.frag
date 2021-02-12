#version 450
#include "ubos.glsl"

layout(location = 0) out vec4 color;

layout (location = 0) in vec2 uv;

layout(set = 3, binding = 0) uniform sampler2D cloudLowResTexture;
layout(set = 3, binding = 1) uniform sampler2D previousFrameTexture;

void main()
{
	// We need to check if the current pixel was rendered to the low res buffer
	// If so we use that value for the pixel because it's the most up to date value
	// Otherwise we reproject the pixel using the result from the previous frame if it is inside the screen
	// and if it is outside we use the result from the up to date low res buffer
	
	vec2 scaledUV = floor(uv * textureSize(previousFrameTexture, 0));
	vec2 cloudLowResDim = textureSize(cloudLowResTexture, 0);
	vec2 uv2 = (floor(uv * cloudLowResDim) + 0.5) / cloudLowResDim;

	float x = mod(scaledUV.x, float(cloudUpdateBlockSize));
	float y = mod(scaledUV.y, float(cloudUpdateBlockSize));
	float frame = y * cloudUpdateBlockSize+ x;

	if(frame == frameNumber)
	{
		//color = vec4(0.0, 0.0,1.0, 1.0);
		color = texture(cloudLowResTexture, uv2);
	}
	else
	{
		vec4 worldPos = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
		worldPos = invProj * worldPos;
		worldPos /= worldPos.w;
		worldPos = invView * worldPos;
	
		vec4 prevFramePos = projectionMatrix * previousFrameView * vec4(worldPos.xyz,1.0);
		prevFramePos.xy /= prevFramePos.w;
		prevFramePos.xy = prevFramePos.xy * 0.5 + 0.5;		// No need to flip the y because the uv.y is already flipped in the vertex shader
	
		//bool isOut = any(greaterThan(abs(prevFramePos.xy - vec2(0.5)), vec2(0.5)));
		if(prevFramePos.x < 0.0 || prevFramePos.x > 1.0 || prevFramePos.y < 0.0 || prevFramePos.y > 1.0)
		{
			//color = vec4(1.0,0.0,0.0,1.0);
			color = texture(cloudLowResTexture, uv);
		}
		else
		{
			//color = vec4(0.0, 1.0, 0.0, 1.0);
			color = texture(previousFrameTexture, prevFramePos.xy);
		}
	}
	//color = texture(cloudLowResTexture, uv);
}