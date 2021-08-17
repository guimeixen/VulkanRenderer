#version 450
#extension GL_GOOGLE_include_directive : enable
#include "ubos.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 worldPos;
layout(location = 1) in vec4 clipSpacePos;

/*layout(set = 1, binding = 0) uniform sampler2D reflectionTex;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D refractionTex;
layout(set = 1, binding = 3) uniform sampler2D refractionDepthTex;
layout(set = 1, binding = 4) uniform sampler2D foamTexture;*/

const float twoPI = 2 * 3.14159;
const float wavelength[4] = float[](13.0, 9.9, 7.3, 6.0);
const float amplitude[4] = float[](0.2, 0.12, 0.08, 0.02);
const float speed[4] = float[](3.4, 2.8, 1.8, 0.6);
const vec2 dir[4] = vec2[](vec2(1.0, -0.2), vec2(1.0, 0.6), vec2(-0.2, 1.0), vec2(-0.43, -0.8));

float LinearizeDepth(float depth)
{
	return 2.0 * nearFarPlane.x * nearFarPlane.y / (nearFarPlane.y + nearFarPlane.x - (2.0 * depth - 1.0) * (nearFarPlane.y - nearFarPlane.x));
}

vec3 waveNormal(vec2 pos)
{
	float x = 0.0;
	float z = 0.0;
	float y = 0.0;
		
    for (int i = 0; i < 4; ++i)
	{
		float frequency = twoPI / wavelength[i];
		float phase = speed[i] * frequency;
		float q = 0.98 / (frequency * amplitude[i] * 4);
		
		float DdotPos = dot(dir[i], pos);
		float c = cos(frequency * DdotPos + phase * timeElapsed);
		float s = sin(frequency * DdotPos + phase * timeElapsed);
	
		float term2 = frequency * amplitude[i] * c;
		x += dir[i].x * term2;
		z += dir[i].y * term2;
		y += q * frequency * amplitude[i] * s;
	}

    return normalize(vec3(-x, 1 - y, -z));
}

void main()
{
	vec3 N = waveNormal(worldPos.xz);
	/*vec3 V = normalize(camPos.xyz - worldPos);
	vec3 H = normalize(V + dirAndIntensity.xyz);
	
	vec2 tex1 = (worldPos.xz * 0.055 + waterNormalMapOffset.xy);
	vec2 tex2 = (worldPos.xz * 0.02 + waterNormalMapOffset.zw);

	vec3 normal1 = texture(normalMap, tex1).rbg * 2.0 - 1.0;
	vec3 normal2 = texture(normalMap, tex2).rbg * 2.0 - 1.0;
	vec3 fineNormal = normal1 + normal2;

	float detailFalloff = clamp((clipSpacePos.w - 60.0) / 40.0, 0.0, 1.0);
    fineNormal = normalize(mix(fineNormal, vec3(0.0, 2.0, 0.0), clamp(detailFalloff - 8.0, 0.0, 1.0)));
    N = normalize(mix(N, vec3(0.0, 1.0, 0.0), detailFalloff));
	
	// Transform fine normal to world space
    vec3 tangent = cross(N, vec3(0.0, 0.0, 1.0));
    vec3 bitangent = cross(tangent, N);
   // N = tangent * fineNormal.x + N * fineNormal.y + bitangent * fineNormal.z;
	N += fineNormal;
	N = normalize(N);
	
	vec2 ndc = clipSpacePos.xy / clipSpacePos.w * 0.5 + 0.5;
	vec2 reflectionUV = vec2(ndc.x, 1.0 - ndc.y);
	vec2 refractionUV = ndc;
	
	vec2 offset = N.xz * vec2(0.05, -0.05);
	reflectionUV += offset;
	refractionUV += offset;
	
	reflectionUV = clamp(reflectionUV, 0.001, 0.999);
	refractionUV = clamp(refractionUV, 0.001, 0.999);
	
	vec3 reflection = texture(reflectionTex, reflectionUV).rgb;
	vec3 refraction = texture(refractionTex, refractionUV).rgb;
	
	float depth = texture(refractionDepthTex, refractionUV).r;
	float waterBottomDistance = LinearizeDepth(depth);
	depth = gl_FragCoord.z;
	float waterTopDistance = LinearizeDepth(depth);
	float waterDepth = waterBottomDistance - waterTopDistance;
	
	refraction = mix(refraction, vec3(0.01, 0.18, 0.44), clamp(waterDepth * 0.3, 0.0, 1.0));
	
	// Fresnel Effect
	float NdotV = max(dot(N, V), 0.0);
	float fresnel = pow(1.0 - NdotV, 5.0);
	fresnel = clamp(fresnel, 0.0, 1.0);

	vec3 specular = pow(max(dot(N, H), 0.0), 256.0) * dirLightColor.xyz;
	
	vec3 foam = texture(foamTexture, worldPos.xz * 0.05).rgb;

	outColor.rgb = mix(refraction, reflection, fresnel);
	outColor.rgb += specular * 3.0;*/
	outColor.rgb=N;
	outColor.a = 1.0;
}
