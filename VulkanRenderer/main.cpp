#include "VKUtils.h"

#include "GLFW/glfw3.h"

#include <iostream>

int main()
{
	bool enableValidationLayers = true;
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

	VkInstance instance;
	VkDebugReportCallbackEXT debugCallback;
	VkSurfaceKHR surface;
	VkPhysicalDevice physicalDevice;
	VkDevice device;

	if (glfwInit() != GLFW_TRUE)
	{
		std::cout << "Failed to initialize GLFW\n";
	}
	else
	{
		std::cout << "GLFW successfuly initialized\n";
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow *window = glfwCreateWindow(640, 480, "VulkanRenderer", nullptr, nullptr);

	if (enableValidationLayers && !vkutils::ValidationLayersSupported(validationLayers))
	{
		std::cout << "Validation layers were requested, but are not available!\n";
		glfwTerminate();
		return 1;
	}

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
		return 1;
	}
	
	std::cout << "Created Vulkan Instance\n";

	if (enableValidationLayers)
	{
		VkDebugReportCallbackCreateInfoEXT callbackInfo = {};
		callbackInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		callbackInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		callbackInfo.pfnCallback = vkutils::DebugCallback;

		if (vkutils::CreateDebugReportCallbackEXT(instance, &callbackInfo, nullptr, &debugCallback) != VK_SUCCESS)
		{
			std::cout << "Failed to create debug report callback!\n";
			return 1;
		}
		std::cout << "Created debug report callback\n";
	}

	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
	{
		std::cout << "Failed to create window surface\n";
		return 1;
	}
	std::cout << "Created window surface\n";

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		std::cout << "Failed to find GPUs with Vulkan support\n";
		return 1;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

	physicalDevice = vkutils::ChoosePhysicalDevice(physicalDevices);

	if (physicalDevice == VK_NULL_HANDLE)
	{
		std::cout << "Failed to find suitable physical device\n";
		return 1;
	}

	VkPhysicalDeviceFeatures deviceFeatures;
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;

	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	std::cout << "GPU: " << deviceProperties.deviceName << '\n';

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}

	vkDestroySurfaceKHR(instance, surface, nullptr);

	if (enableValidationLayers)
		vkutils::DestroyDebugReportCallbackEXT(instance, debugCallback, nullptr);

	vkDestroyInstance(instance, nullptr);

	glfwTerminate();

	return 0;
}