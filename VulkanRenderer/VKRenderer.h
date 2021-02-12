#pragma once

#include "VKBase.h"
#include "VKFramebuffer.h"
#include "Camera.h"
#include "VKTexture3D.h"
#include "UniformBufferTypes.h"

#define CAMERA_SET_BINDING 0
#define GLOBAL_BUFFER_SET_BINDING 1
#define GLOBAL_TEXTURES_SET_BINDING 2
#define USER_TEXTURES_SET_BINDING 3

struct GLFWWindow;

class VKRenderer
{
public:
	VKRenderer();

	bool Init(GLFWwindow* window, unsigned int width, unsigned int height);
	void Dispose();
	void WaitForFrameFences();
	void AcquireNextImage();
	void Present(VkSemaphore graphicsSemaphore, VkSemaphore computeSemaphore);

	void CreateMipMaps(VkCommandBuffer cmdBuffer, const VKTexture2D& texture);
	VkDescriptorSet AllocateUserTextureDescriptorSet();
	VkDescriptorSet AllocateSetFromLayout(VkDescriptorSetLayout layout);
	void UpdateGlobalBuffersSet(const VkDescriptorBufferInfo& info, uint32_t binding, VkDescriptorType descriptorType);
	void UpdateGlobalTexturesSet(const VkDescriptorImageInfo& info, uint32_t binding, VkDescriptorType descriptorType);
	void UpdateUserTextureSet2D(VkDescriptorSet set, const VKTexture2D& texture, unsigned int binding);
	void UpdateUserTextureSet3D(VkDescriptorSet set, const VKTexture3D& texture, unsigned int binding);

	VkCommandBuffer CreateGraphicsCommandBuffer(bool beginRecord);
	VkCommandBuffer CreateComputeCommandBuffer(bool beginRecord);
	void FreeGraphicsCommandBuffer(VkCommandBuffer cmdBuffer);
	void FreeComputeCommandBuffer(VkCommandBuffer cmdBuffer);

	void SetCamera(const Camera &camera);
	void UpdateCameraUBO();
	void UpdateFrameUBO(const FrameUBO &frameData);

	void BeginCmdRecording();
	void BeginDefaultRenderPass();
	void BeginRenderPass(VkCommandBuffer cmdBuffer, const VKFramebuffer& fb, uint32_t clearValueCount, const VkClearValue *clearValues);
	void EndDefaultRenderPass();
	void EndCmdRecording();
	void AcquireImageBarrier(VkCommandBuffer cmdBuffer, const VKTexture2D& texture, int srcQueueFamilyIndex, int dstQueueFamilyIndex);
	void ReleaseImageBarrier(VkCommandBuffer cmdBuffer, const VKTexture2D& texture, int srcQueueFamilyIndex, int dstQueueFamilyIndex);

	VKBase& GetBase() { return base; }
	VkRenderPass GetDefaultRenderPass() const { return renderPass; }
	const std::vector<VkFramebuffer> GetFramebuffers() const { return framebuffers; }
	VkCommandBuffer GetCurrentCmdBuffer() const { return cmdBuffers[currentFrame]; }
	unsigned int GetCurrentFrame() const { return currentFrame; }
	VkSemaphore GetRenderFinishedSemaphore() const { return renderFinishedSemaphores[currentFrame]; }
	VkSemaphore GetImageAvailableSemaphore() const { return presentFinishedSemaphores[currentFrame]; }

	VkPipelineLayout GetPipelineLayout() const { return pipelineLayout; }
	VkDescriptorSet GetGlobalBuffersSet() const { return frameResources[currentFrame].globalBuffersSet; }
	VkDescriptorSet GetGlobalTexturesSet() const { return globalTexturesSet; }

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }

private:
	bool CreateRenderPass();
	bool CreateFramebuffers();

private:
	static const int MAX_FRAMES_IN_FLIGHT = 2;
	const unsigned int MAX_CAMERAS = 4;
	unsigned int currentFrame;
	unsigned int width;
	unsigned int height;

	struct FrameResources {
		VkDescriptorSet camerasSet;
		VkDescriptorSet globalBuffersSet;
	};

	VKBase base;
	VkRenderPass renderPass;
	VKTexture2D depthTexture;
	uint32_t imageIndex;

	FrameResources frameResources[MAX_FRAMES_IN_FLIGHT];

	std::vector<VkFramebuffer> framebuffers;
	std::vector<VkSemaphore> presentFinishedSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> frameFences;
	std::vector<VkFence> imagesInFlight;
	std::vector<VkCommandBuffer> cmdBuffers;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout camerasSetLayout;
	VkDescriptorSetLayout globalBuffersSetLayout;
	VkDescriptorSetLayout globalTexturesSetLayout;
	VkDescriptorSetLayout userTexturesSetLayout;
	VkDescriptorSet globalTexturesSet;
	VkPipelineLayout pipelineLayout;

	VKBuffer cameraUBO;
	glm::mat4* camerasData;
	unsigned int currentCamera;
	unsigned int singleCameraUBOAlignedSize;
	unsigned int allCamerasAlignedSize;

	VKBuffer frameUBO;
	unsigned int singleFrameUBOAlignedSize;
	
};

