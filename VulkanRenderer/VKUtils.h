#pragma once

#include <vulkan\vulkan.h>

#include <vector>

namespace vkutils
{
	struct QueueFamilyIndices {
		int graphicsFamilyIndex = -1;
		int presentFamilyIndex = -1;
		int transferFamilyIndex = -1;
		int computeFamilyIndex = -1;

		bool IsComplete()
		{
			return graphicsFamilyIndex >= 0 && presentFamilyIndex >= 0 && transferFamilyIndex >= 0 && computeFamilyIndex >= 0;
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

	VkPhysicalDevice ChoosePhysicalDevice(const std::vector<VkPhysicalDevice>& physicalDevices, const std::vector<const char*>& deviceExtensions);
	bool CheckPhysicalDeviceExtensionSupport(VkPhysicalDevice physicalDevice, const std::vector<const char*>& deviceExtensions);

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, bool tryFindTransferOnlyQueue, bool tryFindComputeOnlyQueue);

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
	VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);

	uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties& memProps, uint32_t typeFilter, VkMemoryPropertyFlags properties);
	VkFormat FindSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags flags);
	VkFormat FindSupportedDepthFormat(VkPhysicalDevice physicalDevice);
	bool FormatHasStencil(VkFormat format);
	bool IsFormatFilterable(VkPhysicalDevice physicalDevice, VkFormat format, VkImageTiling tiling);
}