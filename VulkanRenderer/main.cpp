#include "VKUtils.h"

#include "GLFW/glfw3.h"

#include <iostream>
#include <string>
#include <set>
#include <array>

int main()
{
	bool enableValidationLayers = true;
	bool showAvailableExtensions = false;
	bool showMemoryProperties = false;
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	const int MAX_FRAMES_IN_FLIGHT = 2;
	const uint32_t width = 640;
	const uint32_t height = 480;

	VkInstance instance;
	VkDebugReportCallbackEXT debugCallback;
	VkSurfaceKHR surface;
	VkSurfaceFormatKHR surfaceFormat;
	VkExtent2D surfaceExtent;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	vkutils::QueueFamilyIndices queueIndices;
	VkQueue graphicsQueue;
	VkQueue transferQueue;
	VkQueue presentQueue;
	VkQueue computeQueue;
	VkCommandPool cmdPool;
	VkSwapchainKHR swapChain;
	std::vector<VkImage> swapChainImages;
	std::vector<VkImageView> swapChainImageViews;
	VkRenderPass renderPass;
	std::vector<VkFramebuffer> framebuffers;
	std::vector<VkCommandBuffer> cmdBuffers;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> frameFences;
	std::vector<VkFence> imagesInFlight;

	if (glfwInit() != GLFW_TRUE)
	{
		std::cout << "Failed to initialize GLFW\n";
	}
	else
	{
		std::cout << "GLFW successfuly initialized\n";
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow *window = glfwCreateWindow(width, height, "VulkanRenderer", nullptr, nullptr);

	glfwSetWindowPos(window, 400, 100);

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
		return 1;
	}
	std::cout << "Created Device\n";

	vkGetDeviceQueue(device, queueIndices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, queueIndices.presentFamily, 0, &presentQueue);
	vkGetDeviceQueue(device, queueIndices.transferFamily, 0, &transferQueue);
	vkGetDeviceQueue(device, queueIndices.computeFamily, 0, &computeQueue);

	VkCommandPoolCreateInfo cmdPoolCreateInfo = {};
	cmdPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolCreateInfo.queueFamilyIndex = queueIndices.graphicsFamily;
	cmdPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;			// Command buffers can be rerecorded. Possible flags are VK_COMMAND_POOL_CREATE_TRANSIENT_BIT and VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT

	if (vkCreateCommandPool(device, &cmdPoolCreateInfo, nullptr, &cmdPool) != VK_SUCCESS)
	{
		std::cout << "Failed to create command pool\n";
		return 1;
	}
	std::cout << "Command pool created\n";

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

	vkutils::SwapChainSupportDetails swapChainSupport = vkutils::QuerySwapChainSupport(physicalDevice, surface);

	surfaceFormat = vkutils::ChooseSwapChainSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = vkutils::ChooseSwapChainPresentMode(swapChainSupport.presentModes);
	surfaceExtent = vkutils::ChooseSwapChainExtent(swapChainSupport.capabilities, width, height);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount;

	// maxImageCount == 0 means that there is no limit besides memory requirements which is why we need to check
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

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
	swapChainInfo.clipped = VK_TRUE;		// If true then we don't care about the color of pixels that are obscured, eg. because another window is in front of them. Best performance with it enabled
	swapChainInfo.oldSwapchain = VK_NULL_HANDLE;

	uint32_t queueFamilyIndices[] = { (uint32_t)queueIndices.graphicsFamily, (uint32_t)queueIndices.presentFamily };

	if (queueIndices.graphicsFamily != queueIndices.presentFamily)
	{
		swapChainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapChainInfo.queueFamilyIndexCount = 2;
		swapChainInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;		// Best performance
		swapChainInfo.queueFamilyIndexCount = 0;
		swapChainInfo.pQueueFamilyIndices = nullptr;
	}

	if (vkCreateSwapchainKHR(device, &swapChainInfo, nullptr, &swapChain) != VK_SUCCESS)
	{
		std::cout << "Failed to create swapchain\n";
		return 1;
	}
	std::cout << "Swapchain created\n";

	uint32_t swapChainImageCount = 0;
	vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, nullptr);
	swapChainImages.resize(swapChainImageCount);
	vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, swapChainImages.data());

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

	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = surfaceFormat.format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Sub passes
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;										// Which attachment to reference by its index in the attachment descriptions array. We only have one so its 0
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// Which layout we would like the attachment to have during a subpass that uses this reference. Vulkan automatically transition the attachment
																			// to this layout when the subpass is started. We intend to use the attachment as a color buffer and VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
																			// layout will give us the best performance.

	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorAttachmentRef;

	// Subpasses in a render pass automatically take care of image layout transitions. These transitions are controlled by subpass dependencies
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;					// We need to wait for the swap chain to finish reading from the image before we can access it
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 1> attachments = { colorAttachment };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDesc;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		std::cout << "Failed to create render pass\n";
		return 1;
	}
	std::cout << "Created render pass\n";

	// Frambuffer
	framebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		VkImageView attachments[] = { swapChainImageViews[i] };

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = surfaceExtent.width;
		framebufferInfo.height = surfaceExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
		{
			std::cout << "Failed to create swapchain framebuffers\n";
			return 1;
		}
	}
	std::cout << "Created framebuffers\n";

	

	cmdBuffers.resize(framebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = cmdPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)cmdBuffers.size();

	if (vkAllocateCommandBuffers(device, &allocInfo, cmdBuffers.data()) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate command buffers!\n";
		return 1;
	}

	for (size_t i = 0; i < cmdBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(cmdBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			std::cout << "Failed to begin recording command buffer!\n";
			return 1;
		}

		VkClearValue clearColor = { 1.0f, 0.0f, 0.0f, 1.0f };

		VkRenderPassBeginInfo renderBeginPassInfo = {};
		renderBeginPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderBeginPassInfo.renderPass = renderPass;
		renderBeginPassInfo.framebuffer = framebuffers[i];
		renderBeginPassInfo.renderArea.offset = { 0, 0 };
		renderBeginPassInfo.renderArea.extent = surfaceExtent;	
		renderBeginPassInfo.clearValueCount = 1;
		renderBeginPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(cmdBuffers[i], &renderBeginPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdEndRenderPass(cmdBuffers[i]);

		if (vkEndCommandBuffer(cmdBuffers[i]) != VK_SUCCESS)
		{
			std::cout << "Failed to record command buffer!\n";
			return 1;
		}
	}

	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frameFences.resize(MAX_FRAMES_IN_FLIGHT);
	imagesInFlight.resize(swapChainImages.size(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
		{

			std::cout << "Failed to create semaphore!\n";
			return 1;
		}

		if (vkCreateFence(device, &fenceInfo, nullptr, &frameFences[i]) != VK_SUCCESS)
		{
			std::cout << "Failed to create fence\n";
			return 1;
		}
	}
	std::cout << "Created semaphores\n";

	size_t currentFrame = 0;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		vkWaitForFences(device, 1, &frameFences[currentFrame], VK_TRUE, UINT64_MAX);
		

		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		// Check if a previous frame is using this image, then wait on it's fence
		if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}

		// The image is now in use by this frame
		imagesInFlight[imageIndex] = frameFences[currentFrame];

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;	
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffers[imageIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		vkResetFences(device, 1, &frameFences[currentFrame]);

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFences[currentFrame]) != VK_SUCCESS)
		{
			std::cout << "Failed to submit draw command buffer!\n";
			return 1;
		}

		VkSwapchainKHR swapChains[] = { swapChain };

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		vkQueuePresentKHR(presentQueue, &presentInfo);

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	vkDeviceWaitIdle(device);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device, frameFences[i], nullptr);
	}

	for (size_t i = 0; i < framebuffers.size(); i++)
	{
		vkDestroyFramebuffer(device, framebuffers[i], nullptr);
	}

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		vkDestroyImageView(device, swapChainImageViews[i], nullptr);
	}

	vkDestroyRenderPass(device, renderPass, nullptr);
	vkDestroySwapchainKHR(device, swapChain, nullptr);
	vkDestroyCommandPool(device, cmdPool, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);

	if (enableValidationLayers)
		vkutils::DestroyDebugReportCallbackEXT(instance, debugCallback, nullptr);

	vkDestroyInstance(instance, nullptr);

	glfwTerminate();

	return 0;
}