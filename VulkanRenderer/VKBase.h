#pragma once

#include "VKUtils.h"
#include "VKBuffer.h"

struct GLFWwindow;

class VKBase
{
public:
	VKBase();

	bool Init(GLFWwindow *window, unsigned int width, unsigned int height, bool enableValidationLayers);
	void Dispose();

	void RecreateSwapchain(unsigned int width, unsigned int height);
	void CopyBuffer(const VKBuffer &srcBuffer, const VKBuffer &dstBuffer, unsigned int size);
	bool CopyBufferToImage(const VKBuffer& buffer, VkImage image, unsigned int width, unsigned int height);
	bool CopyBufferToImage3D(const VKBuffer& buffer, VkImage image, unsigned int width, unsigned int height, unsigned depth);
	bool CopyBufferToCubemapImage(const VKBuffer& buffer, VkImage image, unsigned int width, unsigned int height);
	bool TransitionImageLayout(VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, unsigned int layerCount = 1);
	void TransitionImageLayoutCmdBuffer(VkCommandBuffer cmdBuffer, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, unsigned int layerCount = 1);

	VkInstance GetInstance() const { return instance; }
	VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
	VkDevice GetDevice() const { return device; }
	VkSurfaceKHR GetSurface() const { return surface; }
	VkQueue GetPresentQueue() const { return graphicsQueue; }
	VkQueue GetGraphicsQueue() const { return graphicsQueue; }
	VkQueue GetComputeQueue() const { return computeQueue; }
	VkQueue GetTransferQueue() const { return transferQueue; }
	VkSwapchainKHR GetSwapchain() const { return swapchain; }
	VkCommandPool GetGraphicsCommandPool() const { return graphicsCmdPool; }
	VkCommandPool GetComputeCommandPool() const { return computeCmdPool; }

	const VkPhysicalDeviceMemoryProperties &GetPhysicalDeviceMemoryProperties() const { return physicalDeviceMemoryProperties; }
	const VkPhysicalDeviceLimits& GetPhysicalDeviceLimits() const { return physicalDeviceProperties.limits; }
	const vkutils::QueueFamilyIndices& GetQueueFamilyIndices() const { return queueIndices; }

	VkExtent2D GetSurfaceExtent() const { return surfaceExtent; }
	VkSurfaceFormatKHR GetSurfaceFormat() const { return surfaceFormat; }
	const std::vector<VkImageView> GetSwapchainImageViews() const { return swapChainImageViews; }
	uint32_t GetSwapchainImageCount() const { return static_cast<uint32_t>(swapChainImages.size()); }

private:
	bool CreateInstance();
	bool CreateDebugReportCallback();
	bool CreateSurface(GLFWwindow *window);
	bool ChoosePhysicalDevice();
	bool CreateDevice(VkSurfaceKHR surface);
	bool CreateSwapchain(unsigned int width, unsigned int height);
	bool CreateGraphicsCommandPool();
	bool CreateComputeCommandPool();

	VkCommandBuffer BeginSingleUseCmdBuffer();
	bool EndSingleUseCmdBuffer(VkCommandBuffer cmdBuffer);

private:
	bool enableValidationLayers;
	bool showAvailableExtensions;
	bool showMemoryProperties;
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkInstance instance;
	VkDebugReportCallbackEXT debugCallback;
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties physicalDeviceProperties;
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	VkDevice device;
	vkutils::QueueFamilyIndices queueIndices;
	VkQueue graphicsQueue;
	VkQueue transferQueue;
	VkQueue presentQueue;
	VkQueue computeQueue;
	VkSurfaceKHR surface;
	VkSurfaceFormatKHR surfaceFormat;
	VkExtent2D surfaceExtent;
	VkSwapchainKHR swapchain;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;

	VkCommandPool graphicsCmdPool;
	VkCommandPool computeCmdPool;
};
