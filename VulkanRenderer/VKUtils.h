#pragma once

#include <vulkan\vulkan.h>

#include <vector>

namespace vkutils
{
	struct QueueFamilyIndices {
		int graphicsFamily = -1;
		int presentFamily = -1;
		int transferFamily = -1;
		int computeFamily = -1;

		bool IsComplete()
		{
			return graphicsFamily >= 0 && presentFamily >= 0 && transferFamily >= 0 && computeFamily >= 0;
		}
	};

	struct SwapChainSupportDetails {
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	bool ValidationLayersSupported(const std::vector<const char*>& validationLayers);
	std::vector<const char*> GetRequiredExtensions(bool enableValidationLayers);

	VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData);
	VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);
	void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);

	VkPhysicalDevice ChoosePhysicalDevice(const std::vector<VkPhysicalDevice>& physicalDevices);

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
	VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
}