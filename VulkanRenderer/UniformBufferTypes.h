#pragma once

#include "glm/glm.hpp"

struct alignas(16) CameraUBO
{
	glm::mat4 proj;
	glm::mat4 view;
	glm::mat4 projView;
	glm::mat4 invView;
	glm::mat4 invProj;
	glm::vec4 clipPlane;
	glm::vec4 camPos;
	glm::vec2 nearFarPlane;
};

struct alignas(16) FrameUBO
{
	glm::mat4 orthoProjX;
	glm::mat4 orthoProjY;
	glm::mat4 orthoProjZ;
	glm::mat4 previousFrameView;
	glm::mat4 cloudsInvProjJitter;
	glm::mat4 projGridViewFrame;
	glm::vec4 viewCorner0;
	glm::vec4 viewCorner1;
	glm::vec4 viewCorner2;
	glm::vec4 viewCorner3;
	glm::vec4 waterNormalMapOffset;
	// vec4
	float timeElapsed;
	float giIntensity;
	float skyColorMultiplier;
	float aoIntensity;
	// vec4
	float voxelGridSize;
	float voxelScale;
	int enableGI;
	float timeOfDay;

	// vec4
	float bloomIntensity;
	float bloomThreshold;
	glm::vec2 windDirStr;

	// vec4
	glm::vec4 lightShaftsParams;

	// vec4
	glm::vec4 lightShaftsColor;

	// vec4
	glm::vec2 lightScreenPos;
	glm::vec2 fogParams;

	// vec4

	glm::vec4 fogInscatteringColor;

	//
	glm::vec4 lightInscatteringColor;

	//vec4
	float cloudCoverage;
	float cloudStartHeight;
	float cloudLayerThickness;
	float cloudLayerTopHeight;

	//vec4
	float timeScale;
	float hgForward;
	float densityMult;
	float ambientMult;

	//vec4
	float directLightMult;
	float detailScale;
	float highCloudsCoverage;
	float highCloudsTimeScale;

	//vec4
	float silverLiningIntensity;
	float silverLiningSpread;
	float forwardSilverLiningIntensity;
	float lightShaftsIntensity;

	glm::vec4 ambientTopColor;
	glm::vec4 ambientBottomColor;

	glm::vec2 screenRes;
	//glm::vec2 invScreenRes;
	glm::vec2 vignetteParams;

	glm::vec4 terrainEditParams;
	glm::vec4 terrainEditParams2;

	unsigned int frameNumber;
	unsigned int cloudUpdateBlockSize;
	float deltaTime;
	float padding;
};

struct alignas(16) DirLightUBO
{
	glm::mat4 lightSpaceMatrix[4];
	glm::vec4 dirAndIntensity;		// xyz - dir, w - intensity
	glm::vec4 dirLightColor;		// xyz - color, a - ambient
	glm::vec4 cascadeEnd;
	glm::vec3 skyColor;
};
