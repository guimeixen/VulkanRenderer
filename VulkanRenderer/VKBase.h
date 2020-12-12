#pragma once

#include "VKUtils.h"

#include <GLFW/glfw3.h>

class VKBase
{
public:
	VKBase();

	bool Init(GLFWwindow *window, bool enableValidationLayers);
	void Dispose();

	VkInstance GetInstance() const { return instance; }
	VkPhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
	VkDevice GetDevice() const { return device; }
	VkSurfaceKHR GetSurface() const { return surface; }
	VkQueue GetPresentQueue() const { return graphicsQueue; }
	VkQueue GetGraphicsQueue() const { return graphicsQueue; }

private:
	bool CreateInstance();
	bool CreateDebugReportCallback();
	bool CreateSurface(GLFWwindow *window);
	bool ChoosePhysicalDevice();
	bool CreateDevice(VkSurfaceKHR surface);

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
	
};
