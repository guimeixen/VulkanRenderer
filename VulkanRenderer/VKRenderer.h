#pragma once

#include "VKBase.h"
#include "VKTexture2D.h"

struct GLFWWindow;

class VKRenderer
{
public:
	VKRenderer();

	bool Init(GLFWwindow* window, unsigned int width, unsigned int height);
	void Dispose();
	void BeginFrame();
	void EndFrame();

	void BeginCmdRecording();
	void BeginDefaultRenderPass();
	void EndDefaultRenderPass();
	void EndCmdRecording();

	VKBase& GetBase() { return base; }
	VkRenderPass GetDefaultRenderPass() const { return renderPass; }
	const std::vector<VkFramebuffer> GetFramebuffers() const { return framebuffers; }
	VkCommandBuffer GetCurrentCmdBuffer() const { return cmdBuffers[currentFrame]; }
	unsigned int GetCurrentFrame() const { return currentFrame; }

private:
	bool CreateRenderPass(const VKBase& base, VkRenderPass& renderPass, VkFormat depthFormat);
	bool CreateFramebuffers(const VKBase& base, VkRenderPass renderPass, std::vector<VkFramebuffer>& framebuffers, VkImageView depthImageView);

private:
	const int MAX_FRAMES_IN_FLIGHT = 2;
	unsigned int currentFrame;
	unsigned int width;
	unsigned int height;

	VKBase base;
	VkRenderPass renderPass;
	VKTexture2D depthTexture;
	uint32_t imageIndex;
	std::vector<VkFramebuffer> framebuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> frameFences;
	std::vector<VkFence> imagesInFlight;
	std::vector<VkCommandBuffer> cmdBuffers;
};

