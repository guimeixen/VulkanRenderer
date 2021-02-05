#version 450
#include "ubos.glsl"

layout(location = 0) out vec4 color;

layout(location = 0) in vec2 uv;
layout(location = 1) in vec3 camRay;

/*tex3D_u(0) baseNoiseTexture;
tex3D_u(1) highFreqNoiseTexture;
tex2D_u(2) weatherTexture;*/

layout(set = 2, binding = 0) uniform sampler3D baseNoiseTexture;
layout(set = 2, binding = 1) uniform sampler3D highFreqNoiseTexture;
layout(set = 2, binding = 2) uniform sampler2D weatherTexture;



const vec4 stratus = vec4(0.0, 0.05, 0.1, 0.2);
const vec2 cumulus = vec2(0.0, 0.45);
const vec2 cumulonimbus = vec2(0.0, 1.0);

const float planetSize = 350000.0;
const vec3 planetCenter =vec3(0.0, -planetSize, 0.0);
const float maxSteps = 128.0;
const float minSteps = 64.0;
const float baseScale = 0.00006;

vec3 raySphere(vec3 sc, float sr, vec3 ro, vec3 rd)
{
    vec3 oc = ro - sc;
    float b = dot(rd, oc);
    float c = dot(oc, oc) - sr*sr;
    float t = b*b - c;
    if( t > 0.0) 
        t = -b - sqrt(t);
    return ro + (c/t) * rd;
}

uint calcRaySphereIntersection(vec3 rayOrigin, vec3 rayDirection, vec3 sphereCenter, float radius, out vec2 t)
{
	vec3 l = rayOrigin - sphereCenter;
	float a = 1.0;
	float b = 2.0 * dot(rayDirection, l);
	float c = dot(l, l) - radius * radius;
	float discriminant = b * b - 4.0 * a * c;
	if (discriminant < 0.0)
	{
		t.x = t.y = 0.0;
		return 0;
	}
	else if (abs(discriminant) - 0.00005 <= 0.0)
	{
		t.x = t.y = -0.5 * b / a;
		return 1;
	}
	else
	{
		float q = b > 0.0 ? -0.5 * (b + sqrt(discriminant)) : -0.5 * (b - sqrt(discriminant));

		float h1 = q / a;
		float h2 = c / q;
		t.x = min(h1, h2);
		t.y = max(h1, h2);
		if (t.x < 0.0)
		{
			t.x = t.y;
			if (t.x < 0.0)
			{
				return 0;
			}
			return 1;
		}
		return 2;
	}
}

float remap(float original_value, float original_min, float original_max, float new_min, float new_max)
{
	return new_min + (((original_value - original_min) / (original_max - original_min)) * (new_max - new_min));
}

float beerTerm(float density)
{
	return exp(-density * densityMult);
}

float powderEffect(float density, float cosAngle)
{
	return 1.0 - exp(-density * 2.0 * densityMult);
}

float HenyeyGreensteinPhase(float cosAngle, float g)
{
	float g2 = g * g;
	return ((1.0 - g2) / pow(1.0 + g2 - 2.0 * g * cosAngle, 1.5)) / 4.0 * 3.1415;
}

float SampleNoise(vec3 pos, float coverage, float cloudType, float height_0to1)
{
	pos.y *= baseScale;
	pos.x = pos.x * baseScale + timeElapsed * timeScale;
	pos.z = pos.z * baseScale - timeElapsed * timeScale;
	pos.y -= timeElapsed * timeScale * 0.3;																				// Vertical motion
	//pos += height_0to1 * height_0to1 * height_0to1 * normalize(vec3(1.0, 0.0, 1.0)) * 0.3 * cloudType;				// Shear
	
	float height_1to0 = 1.0 - height_0to1;	
	vec4 noise = textureLod(baseNoiseTexture, pos, 0).rgba;
	
	//float verticalCoverage = textureLod(verticalCoverageTexture, vec2(cloudType, 1.0-height_0to1), 0).r;  // y is flipped
	
	float lowFreqFBM = noise.g * 0.625 + noise.b * 0.25 + noise.a * 0.125;
	float cloud = noise.r * lowFreqFBM;
	cloud *= coverage;
	cloud *= cloudCoverage;
	//cloud *= remap(height_0to1, stratus.x, stratus.y, 0.0, 1.0) * remap(height_0to1, stratus.z, stratus.w, 1.0, 0.0);
	
	//cloud *= height_0to1  * 16.0 * coverage * cloudCoverage;
	
	vec3 detailCoord = pos * detailScale;	
	vec3 highFreqNoise = textureLod(highFreqNoiseTexture, detailCoord, 0).rgb;
	float highFreqFBM = highFreqNoise.r * 0.625 + highFreqNoise.g * 0.25 + highFreqNoise.b * 0.125;

	cloud = remap(cloud, 1.0 - highFreqFBM * height_1to0, 1.0, 0.0, 1.0);							// Erode the edges of the clouds. Multiply by height_1to0 to erode more at the bottom and preserve the tops
	cloud +=  12.0 * cloudCoverage * coverage * cloudCoverage;
	cloud *= smoothstep(0.0, 0.1, height_0to1);					// Smooth the bottoms
	//cloud -= smoothstep(0.0, 0.25, height_0to1);				// More like elevated convection
	//cloud *= verticalCoverage;
	
	cloud = clamp(cloud, 0.0, 1.0);

	return cloud;
}

vec2 sampleWeather(vec3 pos)
{
	pos.x = pos.x * 0.000045 + timeElapsed * timeScale;
	pos.z = pos.z * 0.000045 - timeElapsed * timeScale;
	return textureLod(weatherTexture, pos.xz, 0).rg;
}

float getNormalizedHeight(vec3 pos)
{
	return (distance(pos,  planetCenter) - (planetSize + cloudStartHeight)) / cloudLayerThickness;
}

vec4 clouds(vec3 dir)
{
	vec3 rayStart = vec3(0.0);
	vec3 rayEnd = vec3(0.0);
	vec3 rayPos = vec3(0.0);
	
	//rayStart  = raySphere(planetCenter, planetSize + cloudStartHeight, camPos.xyz, dir);
	//rayEnd = raySphere(planetCenter, planetSize + cloudLayerTopHeight, camPos.xyz, dir);
	//rayEnd = InternalRaySphereIntersect(planetSize + cloudLayerTopHeight, camPos.xyz, dir);

	float distanceCamPlanet = distance(camPos.xyz, planetCenter);
	
	vec2 ih = vec2(0.0);
	vec2 oh = vec2(0.0);
	
	uint innerShellHits = calcRaySphereIntersection(camPos.xyz, dir, planetCenter, planetSize + cloudStartHeight, ih);
	uint outerShellHits = calcRaySphereIntersection(camPos.xyz, dir, planetCenter, planetSize + cloudLayerTopHeight, oh);
	
	vec3 innerHit = camPos.xyz + dir * ih.x;
	vec3 outerHit = camPos.xyz + dir * oh.x;
	
	// Below the cloud layer
	if (distanceCamPlanet < planetSize + cloudStartHeight)
	{
		rayStart = innerHit;
		// Don't march if the ray is below the horizon
		if(rayStart.y < 0.0)
			return vec4(0.0);
			
		rayEnd = outerHit;
	}
	// Above the cloud layer
	/*else if (distanceCamPlanet > planetSize + cloudLayerTopHeight)
	{
		// We can enter the outer shell and leave through the inner shell or enter the outer shell and leave through the outer shell
		rayStart = outerHit;
		// Don't march iif we don't hit the outer shell
		if (outerShellHits == 0)
			return vec4(0.0);
		rayEnd = outerShellHits == 2 && innerShellHits == 0 ? camPos.xyz + dir * oh.y : innerHit;
	}
	// In the cloud layer
	else
	{
		rayStart = camPos.xyz;
		rayEnd = innerShellHits > 0 ? innerHit : outerHit;
	}*/
	
	float steps = int(mix(maxSteps, minSteps, dir.y));		// Take more steps when the ray is pointing more towards the horizon
	float stepSize = distance(rayStart, rayEnd) / steps;
	
	const float largeStepMult = 3.0;
	float stepMult = 1.0;
	
	rayPos = rayStart;
	
	//const float cosAngle = dot(dir, dirAndIntensity.xyz);
	const float cosAngle = dot(dir, normalize(vec3(1.0, 1.0, 0.0)));
	vec4 result = vec4(0.0);
	
	for (float i = 0.0; i < steps; i += stepMult)
	{
		if (result.a >= 0.99)
			break;
			
		float heightFraction = getNormalizedHeight(rayPos);
		
		vec2 weatherData = sampleWeather(rayPos);
		float density = SampleNoise(rayPos, weatherData.r, weatherData.g, heightFraction);
		
		if (density > 0.0)
		{		
			float height_0to1_Light = 0.0;
			float densityAlongLight = 0.0;
			
			//vec3 rayStep = dirAndIntensity.xyz * 40.0;
			vec3 rayStep = normalize(vec3(1.0, 1.0, 0.0)) * 40.0;
			vec3 pos  = rayPos + rayStep;
			
			float thickness = 0.0;
			float scale = 1.0;
			
			for (int s = 0; s < 5; s++)
			{
				pos += rayStep * scale;
				vec2 weatherData = sampleWeather(pos);
				height_0to1_Light = getNormalizedHeight(pos);
				densityAlongLight = SampleNoise(pos, weatherData.r, weatherData.g, height_0to1_Light);
				densityAlongLight *= float(height_0to1_Light <= 1.0);
				thickness += densityAlongLight;
				scale *= 4.0;
			}
			
			float direct = beerTerm(thickness) * powderEffect(density, cosAngle);
			//float HG = mix(HenyeyGreensteinPhase(cosAngle, hgForward) , HenyeyGreensteinPhase(cosAngle, hgBackward), 0.5);		// To make facing away from the sun more interesting
			float hg = max(HenyeyGreensteinPhase(cosAngle, hgForward) * forwardSilverLiningIntensity, silverLiningIntensity * HenyeyGreensteinPhase(cosAngle, 0.99 - silverLiningSpread));
			direct *= hg * directLightMult;
		
			//vec3 ambient = mix(ambientBottomColor.rgb, ambientTopColor.rgb, heightFraction) * dirLightColor.w;
			vec3 ambient = mix(ambientBottomColor.rgb, ambientTopColor.rgb, heightFraction) * 1.0;
			
			vec4 lighting = vec4(density);
			//lighting.rgb = direct * dirLightColor.xyz + ambient;
			lighting.rgb = direct * vec3(1.0) + ambient;
			lighting.rgb *= lighting.a;
	
			result = lighting * (1.0 - result.a) + result;
		}
		
		rayPos += stepSize * dir * stepMult;
	}
	
	return result;
}

float linDepth(float depth)
{
	float zN = 2.0 * depth - 1.0;
	float zE = 2.0 * nearFarPlane.x * nearFarPlane.y / (nearFarPlane.y + nearFarPlane.x - zN * (nearFarPlane.y - nearFarPlane.x));

	return zE / nearFarPlane.y;	// Divide by zFar to distribute the depth value over the whole viewing distacne
}

void main()
{
	vec3 dir = normalize(camRay);
	color =  clouds(dir);
	//color = vec4(1.0, 0.4, 0.2, 1.0);
}