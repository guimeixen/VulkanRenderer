#include "VKUtils.h"

#include "GLFW/glfw3.h"

#include <iostream>

namespace vkutils
{
	bool ValidationLayersSupported(const std::vector<const char*>& validationLayers)
	{
		uint32_t layerPropCount = 0;
		vkEnumerateInstanceLayerProperties(&layerPropCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerPropCount);
		vkEnumerateInstanceLayerProperties(&layerPropCount, availableLayers.data());

		// Compare all the layers we want against the available ones. If we didn't find return false
		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layer : availableLayers)
			{
				if (std::strcmp(layerName, layer.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
				return false;
		}

		return true;
	}

	std::vector<const char*> GetRequiredExtensions(bool enableValidationLayers)
	{
		std::vector<const char*> extensions;

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::cout << "Required GLFW extensions:\n";

		for (uint32_t i = 0; i < glfwExtensionCount; i++)
		{
			extensions.push_back(glfwExtensions[i]);
			std::cout << glfwExtensions[i] << '\n';
		}

		if (enableValidationLayers)
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

		return extensions;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
	{
		std::cout << "Validation Layer: " << msg << '\n';

		return VK_FALSE;
	}

	VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback)
	{
		auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");

		if (func != nullptr)
			return func(instance, pCreateInfo, pAllocator, pCallback);
		else
			return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");

		if (func != nullptr)
			func(instance, callback, pAllocator);
	}

	VkPhysicalDevice ChoosePhysicalDevice(const std::vector<VkPhysicalDevice>& physicalDevices, const std::vector<const char*>& deviceExtensions)
	{
		// Try to find a discrete GPU

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

		for (size_t i = 0; i < physicalDevices.size(); i++)
		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);

			if (deviceProperties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && CheckPhysicalDeviceExtensionSupport(physicalDevices[i], deviceExtensions))
			{
				physicalDevice = physicalDevices[i];
				break;
			}
		}

		// If we don't have a discrete GPU, try to find a integrated GPU

		if (physicalDevice == VK_NULL_HANDLE)
		{
			for (size_t i = 0; i < physicalDevices.size(); i++)
			{
				VkPhysicalDeviceProperties deviceProperties;
				vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProperties);

				if (deviceProperties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && CheckPhysicalDeviceExtensionSupport(physicalDevices[i], deviceExtensions))
				{
					physicalDevice = physicalDevices[i];
					break;
				}
			}
		}

		return physicalDevice;
	}

	bool CheckPhysicalDeviceExtensionSupport(VkPhysicalDevice physicalDevice, const std::vector<const char*>& deviceExtensions)
	{
		uint32_t extensionCount = 0;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

		bool supported = false;
		for (size_t i = 0; i < deviceExtensions.size(); i++)
		{
			supported = false;
			for (size_t j = 0; j < availableExtensions.size(); j++)
			{
				if (strcmp(availableExtensions[j].extensionName, deviceExtensions[i]) == 0)
				{
					supported = true;
					break;
				}
			}

			if (!supported)
			{
				std::cout << "Extension: " << deviceExtensions[i] << " requested, but not supported\n";
				return false;
			}
		}

		return true;
	}

	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface, bool tryFindTransferOnlyQueue, bool tryFindComputeOnlyQueue)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		std::cout << "Queue families count: " << queueFamilyCount << '\n';

		VkBool32 presentSupport = false;

		// TODO: Check if graphics and present queue family is the same and prefer it

		for (size_t i = 0; i < queueFamilies.size(); i++)
		{
			std::cout << "Family " << i << " has " << queueFamilies[i].queueCount << " queues "<< "that support ";

			if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				std::cout << "GRAPHICS, ";
				indices.graphicsFamilyIndex = i;
			}
			if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
				std::cout << "TRANSFER, ";
			if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
				std::cout << "COMPUTE, ";
			if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
				std::cout << "SPARSE BINDING, ";

			// Try to find an exclusive queue for transfer if requested
			if (tryFindTransferOnlyQueue && indices.transferFamilyIndex == -1 && queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
			{
				indices.transferFamilyIndex = i;
			}

			// Try using an exclusive queue for compute if requested
			if (tryFindComputeOnlyQueue && indices.computeFamilyIndex == -1 && queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0)
			{
				indices.computeFamilyIndex = i;
			}


			// Check if this queue family supports presentation
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (queueFamilies[i].queueCount > 0 && presentSupport)
			{
				std::cout << "and PRESENT";
				indices.presentFamilyIndex = i;
			}

			std::cout << std::endl;

			if (indices.IsComplete())
				break;
		}
		
		

		// If we didn't find a transfer exlusive queue, then find the first one that supports transfer
		if (indices.transferFamilyIndex == -1)
		{
			for (size_t i = 0; i < queueFamilies.size(); i++)
			{
				if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
				{
					indices.transferFamilyIndex = i;
					break;
				}
			}
		}

		// If we didn't find a compute exlusive queue, then find the first one that supports compute
		if (indices.computeFamilyIndex == -1)
		{
			for (size_t i = 0; i < queueFamilies.size(); i++)
			{
				if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
				{
					indices.computeFamilyIndex = i;
					break;
				}
			}
		}

		return indices;
	}

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details = {};

		// Surface capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		// Surface formats
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		// Surface present modes
		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

		if (presentModeCount != 0)
		{
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		// If the surface has no preferred format then choose the one we want
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		// Else check if the format we want is in the list of available formats
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		// If that fails we could rank the format based on how good they are
		return availableFormats[0];
	}

	VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
	{
		VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)		// Some drivers don't support FIFO so we should prefer Immediate if Mailbox is not available
			{
				bestMode = availablePresentMode;
			}
		}

		return bestMode;
	}

	VkExtent2D ChooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = { width, height };

			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

	uint32_t FindMemoryType(const VkPhysicalDeviceMemoryProperties &memProps, uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
		{
			if (typeFilter & (1 << i) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
				return i;
		}

		return 0;
	}
}
