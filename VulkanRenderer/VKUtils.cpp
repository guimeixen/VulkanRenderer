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

	VkPhysicalDevice ChoosePhysicalDevice(const std::vector<VkPhysicalDevice>& physicalDevices)
	{
		// Try to find a discrete GPU

		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

		for (const VkPhysicalDevice &pd : physicalDevices)
		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(pd, &deviceProperties);

			if (deviceProperties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				physicalDevice = pd;
				break;
			}
		}

		// If we don't have a discrete GPU, try to find a integrated GPU

		if (physicalDevice == VK_NULL_HANDLE)
		{
			for (const VkPhysicalDevice& pd : physicalDevices)
			{
				VkPhysicalDeviceProperties deviceProperties;
				vkGetPhysicalDeviceProperties(pd, &deviceProperties);

				if (deviceProperties.deviceType == VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
				{
					physicalDevice = pd;
					break;
				}
			}
		}

		return physicalDevice;
	}
}
