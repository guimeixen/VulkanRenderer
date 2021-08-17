#pragma once

#include "VKFramebuffer.h"
#include "Water.h"
#include "VolumetricClouds.h"
#include "Skybox.h"
#include "ComputeMaterial.h"
#include "ModelManager.h"
#include "ParticleManager.h"
#include "TransformManager.h"

class Renderer;

class RenderingPath
{
public:
	RenderingPath();

	bool Init(VKRenderer* renderer, unsigned int width, unsigned int height);
	void Update(const Camera& camera, float deltaTime);
	void EndFrame(const Camera& camera);
	bool PerformShadowMapPass(VkCommandBuffer cmdBuffer, ModelManager &modelManager);
	bool PerformVolumetricCloudsPass(VkCommandBuffer cmdBuffer);
	bool PerformHDRPass(VkCommandBuffer cmdBuffer, ModelManager& modelManager, ParticleManager& particleManager);
	bool PerformPostProcessPass(VkCommandBuffer cmdBuffer);
	bool PerformComputePass();
	bool SubmitCompute();
	void UpdateBuffers(const Camera& camera, const ModelManager& modelManager, TransformManager& transformManager, float deltaTime, float timeElapsed);
	
	void Dispose();

	const VKFramebuffer& GetHDRFramebuffer() const { return hdrFB; }

	const VKTexture2D& GetStorageTexture() const { return storageTexture; }

	VkSemaphore GetGraphicsSemaphore() const { return graphicsSemaphore; }
	VkSemaphore GetComputeSemaphore() const { return computeSemaphore; }

private:
	bool CreateShadowMapPass();
	bool CreateHDRPass();
	bool CreatePostProcessPass();
	bool CreateComputePass();

private:
	unsigned int width, height;
	VKRenderer* renderer;
	FrameUBO frameData;
	VkSemaphore computeSemaphore;
	VkSemaphore graphicsSemaphore;

	VKBuffer dirLightUBO;
	VKBuffer instanceDataBuffer;

	Water projectedGridWater;
	VolumetricClouds volClouds;
	Skybox skybox;

	glm::mat4 previousFrameView;

	VKTexture2D storageTexture;

	// Shadow map
	VKFramebuffer shadowFB;
	Mesh shadowMesh;
	Material shadowMat;
	Camera lightSpaceCamera;

	// HDR pass
	VKFramebuffer hdrFB;

	// Post Process
	Mesh postQuadMesh;
	Material postQuadMat;
	VkDescriptorSet postQuadSet;

	// Compute pass
	ComputeMaterial computeMat;
	VkFence computeFence;
	VkDescriptorSet computeSet;
	Mesh quadMesh;
	Material quadMat;
	VkCommandBuffer computeCmdBuffer;
};

