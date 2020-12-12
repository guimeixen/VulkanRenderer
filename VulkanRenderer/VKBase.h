#pragma once

#include "VKUtils.h"

#include <GLFW/glfw3.h>

class VKBase
{
public:
	VKBase();

	bool Init(GLFWwindow *window, unsigned int width, unsigned int height, bool enableValidationLayers);
	void Dispose();

	VkInstance GetInstance() const { return instance; }
	VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
	VkDevice GetDevice() const { return device; }
	VkSurfaceKHR GetSurface() const { return surface; }
	VkQueue GetPresentQueue() const { return graphicsQueue; }
	VkQueue GetGraphicsQueue() const { return graphicsQueue; }
	VkSwapchainKHR GetSwapchain() const { return swapchain; }

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

private:
	bool enableValidationLayers;
	bool showAvailableExtensions;
	bool showMemoryProperties;
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkInstance instance;
	VkDebugReportCallbackEXT debugCallback;
	VkPhysicalDevice physicalDevice;
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
};
