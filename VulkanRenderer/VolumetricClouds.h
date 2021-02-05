#pragma once

#include "VKRenderer.h"
#include "VKTexture3D.h"
#include "Material.h"

#include "glm/glm.hpp"

struct VolumetricCloudsData
{
	float cloudCoverage;
	float cloudStartHeight;
	float cloudLayerThickness;
	float cloudLayerTopHeight;
	float timeScale;
	float hgForward;
	float densityMult;
	float ambientMult;
	float detailScale;
	float highCloudsCoverage;
	float highCloudsTimeScale;
	float silverLiningIntensity;
	float silverLiningSpread;
	float forwardSilverLiningIntensity;
	float directLightMult;
	glm::vec4 ambientTopColor;
	glm::vec4 ambientBottomColor;
};

class VolumetricClouds
{
public:
	VolumetricClouds();

	bool Init(VKRenderer* renderer);
	void Dispose(VkDevice device);
	void PerformCloudsPass(VKRenderer* renderer, VkCommandBuffer cmdBuffer);
	void PerformCloudsReprojectionPass(VKRenderer* renderer, VkCommandBuffer cmdBuffer);
	void EndFrame();

	const VKTexture2D& GetCloudsTexture() const { return cloudsReprojectionFB.GetFirstColorTexture(); }
	VolumetricCloudsData& GetVolumetricCloudsData() { return volCloudsData; }
	unsigned int GetFrameNumber() const { return frameNumber; }
	unsigned int GetUpdateBlockSize() const { return cloudUpdateBlockSize; }
	glm::mat4 GetJitterMatrix() const;

private:
	VKFramebuffer cloudsLowResFB;
	VKFramebuffer cloudsReprojectionFB;
	VKFramebuffer cloudCopyFB;
	VKTexture2D previousFrameTexture[2];
	VKTexture3D baseNoiseTexture;
	VKTexture3D highFreqNoiseTexture;
	VKTexture2D weatherTexture;

	Material cloudMat;
	VkDescriptorSet cloudMatSet;
	Material cloudReprojectionMat;
	VkDescriptorSet cloudReprojectionSet[2];
	Mesh quadMesh;

	VolumetricCloudsData volCloudsData;

	unsigned int cloudsFBWidth;
	unsigned int cloudsFBHeight;
	unsigned int cloudUpdateBlockSize;
	unsigned int frameNumbers[16];
	unsigned int frameNumber;
	unsigned int frameCount;

	int currentFrame = 0;
};

