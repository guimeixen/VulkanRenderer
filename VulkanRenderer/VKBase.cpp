#include "VKBase.h"

#include "VKUtils.h"

#include <iostream>
#include <set>

VKBase::VKBase()
{
	instance = VK_NULL_HANDLE;
	debugCallback = VK_NULL_HANDLE;
	physicalDevice = VK_NULL_HANDLE;
	device = VK_NULL_HANDLE;
	surface = VK_NULL_HANDLE;
	enableValidationLayers = true;
	showAvailableExtensions = false;
	showMemoryProperties = false;

	graphicsQueue = VK_NULL_HANDLE;
	presentQueue = VK_NULL_HANDLE;
	transferQueue = VK_NULL_HANDLE;
	computeQueue = VK_NULL_HANDLE;
}

bool VKBase::Init(GLFWwindow* window, bool enableValidationLayers)
{
	this->enableValidationLayers = enableValidationLayers;

	if (enableValidationLayers && !vkutils::ValidationLayersSupported(validationLayers))
	{
		std::cout << "Validation layers were requested, but are not available!\n";
		return false;
	}

	if (!CreateInstance())
		return false;
	if (!CreateDebugReportCallback())
		return false;
	if (!CreateSurface(window))
		return false;
	if (!ChoosePhysicalDevice())
		return false;
	if (!CreateDevice(surface))
		return false;

	return true;
}

void VKBase::Dispose()
{
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);

	if (enableValidationLayers && debugCallback != VK_NULL_HANDLE)
		vkutils::DestroyDebugReportCallbackEXT(instance, debugCallback, nullptr);

	vkDestroyInstance(instance, nullptr);
}

bool VKBase::CreateInstance()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "VulkanRenderer";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	const std::vector<const char*> requiredExtentions = vkutils::GetRequiredExtensions(enableValidationLayers);

	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtentions.size());
	instanceInfo.ppEnabledExtensionNames = requiredExtentions.data();

	if (enableValidationLayers)
	{
		instanceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		instanceInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		instanceInfo.enabledLayerCount = 0;
	}

	if (vkCreateInstance(&instanceInfo, nullptr, &instance) != VK_SUCCESS)
	{
		std::cout << "Failed to create Vulkan Instance!\n";
		return false;
	}

	std::cout << "Created Instance\n";

	if (showAvailableExtensions)
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::cout << "Available Extensions:\n";
		for (const auto& extension : extensions)
		{
			std::cout << '\t' << extension.extensionName << '\n';
		}
	}

	return true;
}

bool VKBase::CreateDebugReportCallback()
{
	if (enableValidationLayers)
	{
		VkDebugReportCallbackCreateInfoEXT callbackInfo = {};
		callbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		callbackInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		callbackInfo.pfnCallback = vkutils::DebugCallback;

		if (vkutils::CreateDebugReportCallbackEXT(instance, &callbackInfo, nullptr, &debugCallback) != VK_SUCCESS)
		{
			std::cout << "Failed to create debug report callback!\n";
			return false;
		}
		std::cout << "Created debug report callback\n";
	}

	return true;
}

bool VKBase::CreateSurface(GLFWwindow* window)
{
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		std::cout << "Failed to create window surface\n";
		return false;
	}
	std::cout << "Created window surface\n";

	return true;
}

bool VKBase::ChoosePhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		std::cout << "Failed to find GPUs with Vulkan support\n";
		return false;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

	physicalDevice = vkutils::ChoosePhysicalDevice(physicalDevices);

	if (physicalDevice == VK_NULL_HANDLE)
	{
		std::cout << "Failed to find suitable physical device\n";
		return false;
	}

	VkPhysicalDeviceFeatures deviceFeatures;
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;

	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	std::cout << "GPU: " << deviceProperties.deviceName << '\n';
	std::cout << "Max allocations: " << deviceProperties.limits.maxMemoryAllocationCount << '\n';
	std::cout << "Memory heaps: " << deviceMemoryProperties.memoryHeapCount << '\n';
	std::cout << "Max push constant size: " << deviceProperties.limits.maxPushConstantsSize << '\n';

	if (showMemoryProperties)
	{
		for (uint32_t i = 0; i < deviceMemoryProperties.memoryHeapCount; i++)
		{
			if (deviceMemoryProperties.memoryHeaps[i].flags == VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
				std::cout << "\tSize: " << (deviceMemoryProperties.memoryHeaps[i].size >> 20) << " mib - Device Local\n";
			else
				std::cout << "\tSize: " << (deviceMemoryProperties.memoryHeaps[i].size >> 20) << " mib\n";
		}

		std::cout << "Memory types: " << deviceMemoryProperties.memoryTypeCount << '\n';

		std::string str;
		for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++)
		{
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
				str += "Device local, ";
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
				str += "Host visible, ";
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
				str += "Host coherent, ";
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
				str += "Host cached, ";

			std::cout << "\tHeap index: " << deviceMemoryProperties.memoryTypes[i].heapIndex << " - Property Flags: " << str << '\n';
			str.clear();
		}
	}

	return true;
}

bool VKBase::CreateDevice(VkSurfaceKHR surface)
{
	queueIndices = vkutils::FindQueueFamilies(physicalDevice, surface);

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { queueIndices.graphicsFamily, queueIndices.presentFamily, queueIndices.transferFamily };		// If the graphics family and the present family are the same, which is likely

	float queuePriority = 1.0f;

	for (int queueFamilyIndex : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeaturesToEnable = {};

	VkDeviceCreateInfo deviceInfo = {};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceInfo.pEnabledFeatures = &deviceFeaturesToEnable;
	deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

	if (enableValidationLayers)
	{
		deviceInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		deviceInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		deviceInfo.enabledLayerCount = 0;
	}

	if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device) != VK_NULL_HANDLE)
	{
		std::cout << "Failed to create Device\n";
		return false;
	}
	std::cout << "Created Device\n";

	vkGetDeviceQueue(device, queueIndices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, queueIndices.presentFamily, 0, &presentQueue);
	vkGetDeviceQueue(device, queueIndices.transferFamily, 0, &transferQueue);
	vkGetDeviceQueue(device, queueIndices.computeFamily, 0, &computeQueue);

	return true;
}
