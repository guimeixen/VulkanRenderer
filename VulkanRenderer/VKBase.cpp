#include "VKBase.h"

#include "VKUtils.h"

#include <GLFW/glfw3.h>

#include <iostream>
#include <set>

VKBase::VKBase()
{
	instance = VK_NULL_HANDLE;
	debugCallback = VK_NULL_HANDLE;
	physicalDevice = VK_NULL_HANDLE;
	device = VK_NULL_HANDLE;
	surface = VK_NULL_HANDLE;
	swapchain = VK_NULL_HANDLE;
	enableValidationLayers = true;
	showAvailableExtensions = false;
	showMemoryProperties = false;

	cmdPool = VK_NULL_HANDLE;

	physicalDeviceMemoryProperties = {};

	surfaceExtent = {};
	surfaceFormat = {};

	graphicsQueue = VK_NULL_HANDLE;
	presentQueue = VK_NULL_HANDLE;
	transferQueue = VK_NULL_HANDLE;
	computeQueue = VK_NULL_HANDLE;
}

bool VKBase::Init(GLFWwindow* window, unsigned int width, unsigned int height, bool enableValidationLayers)
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
	if (!CreateSwapchain(width, height))
		return false;
	if (!CreateCommandPool())
		return false;

	return true;
}

void VKBase::Dispose()
{
	vkDestroyCommandPool(device, cmdPool, nullptr);

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		vkDestroyImageView(device, swapChainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);

	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);

	if (enableValidationLayers && debugCallback != VK_NULL_HANDLE)
		vkutils::DestroyDebugReportCallbackEXT(instance, debugCallback, nullptr);

	vkDestroyInstance(instance, nullptr);
}

void VKBase::RecreateSwapchain(unsigned int width, unsigned int height)
{
	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		vkDestroyImageView(device, swapChainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);

	CreateSwapchain(width, height);
}

VkCommandBuffer VKBase::BeginSingleUseCmdBuffer()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = cmdPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmdBuffer;
	
	if (vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate command buffer\n";
		return VK_NULL_HANDLE;
	}

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS)
	{
		std::cout << "Failed to begin command buffer\n";
		return VK_NULL_HANDLE;
	}

	return cmdBuffer;
}

bool VKBase::EndSingleUseCmdBuffer(VkCommandBuffer cmdBuffer)
{
	if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS)
	{
		std::cout << "Failed to end command buffer\n";
		return false;
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);

	return true;
}

void VKBase::CopyBuffer(const VKBuffer& srcBuffer, const VKBuffer& dstBuffer, unsigned int size)
{
	VkCommandBuffer cmdBuffer = BeginSingleUseCmdBuffer();

	if (cmdBuffer == VK_NULL_HANDLE)
	{
		std::cout << "Failed to copy buffer, command buffer null handle\n";
		return;
	}

	VkBufferCopy copyRegion = {};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(cmdBuffer, srcBuffer.GetBuffer(), dstBuffer.GetBuffer(), 1, &copyRegion);

	if (!EndSingleUseCmdBuffer(cmdBuffer))
	{
		std::cout << "Failed to end command buffer for image layout transition\n";
		return;
	}
}

bool VKBase::CopyBufferToImage(const VKBuffer& buffer, VkImage image, unsigned int width, unsigned int height)
{
	VkCommandBuffer cmdBuffer = BeginSingleUseCmdBuffer();

	if (cmdBuffer == VK_NULL_HANDLE)
	{
		std::cout << "Failed to copy buffer, command buffer null handle\n";
		return false;
	}

	VkBufferImageCopy copy = {};
	copy.bufferOffset = 0;
	copy.bufferRowLength = 0;
	copy.bufferImageHeight = 0;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.baseArrayLayer = 0;
	copy.imageSubresource.layerCount = 1;
	copy.imageSubresource.mipLevel = 0;
	copy.imageOffset = { 0, 0, 0 };
	copy.imageExtent.width = static_cast<uint32_t>(width);
	copy.imageExtent.height = static_cast<uint32_t>(height);
	copy.imageExtent.depth = 1;

	vkCmdCopyBufferToImage(cmdBuffer, buffer.GetBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);


	if (!EndSingleUseCmdBuffer(cmdBuffer))
	{
		std::cout << "Failed to end command buffer for image layout transition\n";
		return false;
	}

	return true;
}

bool VKBase::CopyBufferToCubemapImage(const VKBuffer& buffer, VkImage image, unsigned int width, unsigned int height)
{
	VkCommandBuffer cmdBuffer = BeginSingleUseCmdBuffer();

	if (cmdBuffer == VK_NULL_HANDLE)
	{
		std::cout << "Failed to copy buffer, command buffer null handle\n";
		return false;
	}

	VkBufferImageCopy copy[6] = {};

	VkDeviceSize offset = 0;

	for (size_t i = 0; i < 6; i++)
	{
		copy[i].bufferOffset = offset;
		copy[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy[i].imageSubresource.baseArrayLayer = i;
		copy[i].imageSubresource.layerCount = 1;
		copy[i].imageSubresource.mipLevel = 0;
		copy[i].imageExtent.width = static_cast<uint32_t>(width);
		copy[i].imageExtent.height = static_cast<uint32_t>(height);
		copy[i].imageExtent.depth = 1;

		offset += static_cast<VkDeviceSize>(width * height * 4 * sizeof(unsigned char)); // Here we assume the cubemap is RGBA8
	}

	vkCmdCopyBufferToImage(cmdBuffer, buffer.GetBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, copy);


	if (!EndSingleUseCmdBuffer(cmdBuffer))
	{
		std::cout << "Failed to end command buffer for image layout transition\n";
		return false;
	}

	return true;
}

bool VKBase::TransitionImageLayout(VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, unsigned int layerCount)
{
	VkCommandBuffer cmdBuffer = BeginSingleUseCmdBuffer();

	if (cmdBuffer == VK_NULL_HANDLE)
	{
		std::cout << "Failed to transition image layout, command buffer null handle\n";
		return false;
	}

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = currentLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = layerCount;
	barrier.subresourceRange.levelCount = 1;

	VkPipelineStageFlags srcStage;
	VkPipelineStageFlags dstStage;

	if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		// We don't need to wait on anything to perform the transfer so specify the earliest stage, mask can be 0
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		// The shader reads in the fragment shader should wait on the transfer writes
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;		// Assuming we first use the image on the fragment shader. Would have to change if used on the vertex shader first
	}
	else if (currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;		// How to know if we're using it in the compute or fragment, vertex, etc? Pass something as parameter?
	}

	vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	if (!EndSingleUseCmdBuffer(cmdBuffer))
	{
		std::cout << "Failed to end command buffer for image layout transition\n";
		return false;
	}

	return true;
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

	physicalDevice = vkutils::ChoosePhysicalDevice(physicalDevices, deviceExtensions);

	if (physicalDevice == VK_NULL_HANDLE)
	{
		std::cout << "Failed to find suitable physical device\n";
		return false;
	}

	VkPhysicalDeviceFeatures deviceFeatures;

	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
	vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);

	std::cout << "GPU: " << physicalDeviceProperties.deviceName << '\n';
	std::cout << "Max allocations: " << physicalDeviceProperties.limits.maxMemoryAllocationCount << '\n';
	std::cout << "Memory heaps: " << physicalDeviceMemoryProperties.memoryHeapCount << '\n';
	std::cout << "Max push constant size: " << physicalDeviceProperties.limits.maxPushConstantsSize << '\n';

	if (showMemoryProperties)
	{
		for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryHeapCount; i++)
		{
			if (physicalDeviceMemoryProperties.memoryHeaps[i].flags == VkMemoryHeapFlagBits::VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
				std::cout << "\tSize: " << (physicalDeviceMemoryProperties.memoryHeaps[i].size >> 20) << " mib - Device Local\n";
			else
				std::cout << "\tSize: " << (physicalDeviceMemoryProperties.memoryHeaps[i].size >> 20) << " mib\n";
		}

		std::cout << "Memory types: " << physicalDeviceMemoryProperties.memoryTypeCount << '\n';

		std::string str;
		for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++)
		{
			if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
				str += "Device local, ";
			if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
				str += "Host visible, ";
			if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
				str += "Host coherent, ";
			if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) == VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
				str += "Host cached, ";

			std::cout << "\tHeap index: " << physicalDeviceMemoryProperties.memoryTypes[i].heapIndex << " - Property Flags: " << str << '\n';
			str.clear();
		}
	}

	return true;
}

bool VKBase::CreateDevice(VkSurfaceKHR surface)
{
	queueIndices = vkutils::FindQueueFamilies(physicalDevice, surface, false, true);

	std::cout << "Graphics queue index: " << queueIndices.graphicsFamilyIndex << '\n';
	std::cout << "Present queue index: " << queueIndices.presentFamilyIndex << '\n';
	std::cout << "Transfer queue index: " << queueIndices.transferFamilyIndex << '\n';
	std::cout << "Compute queue index: " << queueIndices.computeFamilyIndex << '\n';

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> uniqueQueueFamilies = { queueIndices.graphicsFamilyIndex, queueIndices.presentFamilyIndex, queueIndices.transferFamilyIndex, queueIndices.computeFamilyIndex };		// If the graphics family and the present family are the same, which is likely

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

	vkGetDeviceQueue(device, queueIndices.graphicsFamilyIndex, 0, &graphicsQueue);
	vkGetDeviceQueue(device, queueIndices.presentFamilyIndex, 0, &presentQueue);
	vkGetDeviceQueue(device, queueIndices.transferFamilyIndex, 0, &transferQueue);
	vkGetDeviceQueue(device, queueIndices.computeFamilyIndex, 0, &computeQueue);

	return true;
}

bool VKBase::CreateSwapchain(unsigned int width, unsigned int height)
{
	vkutils::SwapChainSupportDetails swapChainSupport = vkutils::QuerySwapChainSupport(physicalDevice, surface);

	if (swapChainSupport.formats.size() == 0)
	{
		std::cout << "0 swapchain formats available\n";
		return false;
	}
	if (swapChainSupport.presentModes.size() == 0)
	{
		std::cout << "0 swapchain present modes available\n";
		return false;
	}

	surfaceFormat = vkutils::ChooseSwapChainSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = vkutils::ChooseSwapChainPresentMode(swapChainSupport.presentModes);
	surfaceExtent = vkutils::ChooseSwapChainExtent(swapChainSupport.capabilities, width, height);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount;

	// maxImageCount == 0 means that there is no limit besides memory requirements which is why we need to check
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	std::cout << "Swapchain image count: " << imageCount << '\n';

	VkSwapchainCreateInfoKHR swapChainInfo = {};
	swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainInfo.surface = surface;
	swapChainInfo.minImageCount = imageCount;
	swapChainInfo.imageFormat = surfaceFormat.format;
	swapChainInfo.imageColorSpace = surfaceFormat.colorSpace;
	swapChainInfo.imageExtent = surfaceExtent;
	swapChainInfo.imageArrayLayers = 1;
	swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainInfo.preTransform = swapChainSupport.capabilities.currentTransform;	// We could flip the images or do other things but we don't want to so use the currentTransform
	swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;			// Used for blending with other windows.
	swapChainInfo.presentMode = presentMode;
	swapChainInfo.clipped = VK_TRUE;
	swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

	uint32_t queueFamilyIndices[] = { (uint32_t)queueIndices.graphicsFamilyIndex, (uint32_t)queueIndices.presentFamilyIndex };

	if (queueIndices.graphicsFamilyIndex != queueIndices.presentFamilyIndex)
	{
		swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainInfo.queueFamilyIndexCount = 2;
		swapChainInfo.pQueueFamilyIndices = queueFamilyIndices;
		std::cout << "Graphics and present queue family different\n";
	}
	else
	{
		swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;		// Best performance
		swapChainInfo.queueFamilyIndexCount = 0;
		swapChainInfo.pQueueFamilyIndices = nullptr;
	}

	if (vkCreateSwapchainKHR(device, &swapChainInfo, nullptr, &swapchain) != VK_SUCCESS)
	{
		std::cout << "Failed to create swapchain\n";
		return 1;
	}
	std::cout << "Swapchain created\n";

	uint32_t swapChainImageCount = 0;
	vkGetSwapchainImagesKHR(device, swapchain, &swapChainImageCount, nullptr);
	swapChainImages.resize(swapChainImageCount);
	vkGetSwapchainImagesKHR(device, swapchain, &swapChainImageCount, swapChainImages.data());

	std::cout << "Created swapchain images\n";

	// Image views
	swapChainImageViews.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = swapChainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = surfaceFormat.format;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device, &viewInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
		{
			std::cout << "Failed to create swapchain image view with index: " << i << '\n';
			return 1;
		}
	}
	std::cout << "Created swapchain image views\n";

	return true;
}

bool VKBase::CreateCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.queueFamilyIndex = queueIndices.graphicsFamilyIndex;
	cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(device, &cmdPoolCreateInfo, nullptr, &cmdPool) != VK_SUCCESS)
	{
		std::cout << "Failed to create command pool\n";
		return false;
	}
	std::cout << "Command pool created\n";

	return true;
}
