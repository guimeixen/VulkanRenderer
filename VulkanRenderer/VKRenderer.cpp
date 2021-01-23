#include "VKRenderer.h"

#include <iostream>
#include <cassert>

VKRenderer::VKRenderer()
{
	renderPass = VK_NULL_HANDLE;
	imageIndex = 0;
	currentFrame = 0;
	width = 0;
	height = 0;
}

bool VKRenderer::Init(GLFWwindow *window, unsigned int width, unsigned int height)
{
	if (!base.Init(window, true, width, height))
		return false;

	this->width = width;
	this->height = height;

	TextureParams depthTextureParams = {};
	depthTextureParams.format = vkutils::FindSupportedDepthFormat(base.GetPhysicalDevice());

	depthTexture.CreateDepthTexture(base, depthTextureParams, base.GetSurfaceExtent().width, base.GetSurfaceExtent().height, false);

	if (!CreateRenderPass(base, renderPass, depthTexture.GetFormat()))
		return false;
	if (!CreateFramebuffers(base, renderPass, framebuffers, depthTexture.GetImageView()))
		return false;

	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frameFences.resize(MAX_FRAMES_IN_FLIGHT);
	imagesInFlight.resize(base.GetSwapchainImageCount(), VK_NULL_HANDLE);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkDevice device = base.GetDevice();

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
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


	cmdBuffers.resize(framebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = base.GetCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)cmdBuffers.size();

	if (vkAllocateCommandBuffers(base.GetDevice(), &allocInfo, cmdBuffers.data()) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate command buffers!\n";
		return false;
	}

	return true;
}

void VKRenderer::Dispose()
{
	VkDevice device = base.GetDevice();
	depthTexture.Dispose(device);

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

	vkDestroyRenderPass(device, renderPass, nullptr);

}

void VKRenderer::WaitForFrameFences()
{
	vkWaitForFences(base.GetDevice(), 1, &frameFences[currentFrame], VK_TRUE, UINT64_MAX);
}

void VKRenderer::Present()
{
	VkDevice device = base.GetDevice();
	VkQueue graphicsQueue = base.GetGraphicsQueue();
	VkQueue presentQueue = base.GetPresentQueue();

	VkResult res = vkAcquireNextImageKHR(device, base.GetSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	assert(imageIndex == currentFrame);

	if (res == VK_SUBOPTIMAL_KHR)
	{
		std::cout << "Swapchain suboptimal\n";
	}
	else if (res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		vkDeviceWaitIdle(device);

		for (size_t i = 0; i < framebuffers.size(); i++)
		{
			vkDestroyFramebuffer(device, framebuffers[i], nullptr);
		}

		vkDestroyRenderPass(device, renderPass, nullptr);

		vkFreeCommandBuffers(device, base.GetCommandPool(), static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());

		base.RecreateSwapchain(width, height);
		CreateRenderPass(base, renderPass, depthTexture.GetFormat());
		CreateFramebuffers(base, renderPass, framebuffers, depthTexture.GetImageView());
		// We can't present so go to the next iteration
		//continue;
	}

	// Check if a previous frame is using this image, then wait on it's fence
	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
	{
		vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}

	// The image is now in use by this frame
	imagesInFlight[imageIndex] = frameFences[currentFrame];

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailableSemaphores[currentFrame];
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

	vkResetFences(device, 1, &frameFences[currentFrame]);

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFences[currentFrame]) != VK_SUCCESS)
	{
		std::cout << "Failed to submit draw command buffer!\n";
	}

	VkSwapchainKHR swapChains[] = { base.GetSwapchain() };

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	res = vkQueuePresentKHR(presentQueue, &presentInfo);

	if (res == VK_SUBOPTIMAL_KHR)
	{
		std::cout << "Swapchain suboptimal\n";
	}
	else if (res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		vkDeviceWaitIdle(device);

		for (size_t i = 0; i < framebuffers.size(); i++)
		{
			vkDestroyFramebuffer(device, framebuffers[i], nullptr);
		}

		vkDestroyRenderPass(device, renderPass, nullptr);

		vkFreeCommandBuffers(device, base.GetCommandPool(), static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());

		base.RecreateSwapchain(width, height);
		CreateRenderPass(base, renderPass, depthTexture.GetFormat());
		CreateFramebuffers(base, renderPass, framebuffers, depthTexture.GetImageView());
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VKRenderer::BeginCmdRecording()
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(cmdBuffers[currentFrame], &beginInfo) != VK_SUCCESS)
	{
		std::cout << "Failed to begin recording command buffer!\n";
	}
}

void VKRenderer::BeginDefaultRenderPass()
{
	VkExtent2D surfaceExtent = base.GetSurfaceExtent();

	VkClearValue clearValues[2] = {};
	clearValues[0].color = { 0.3f, 0.3f, 0.3f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderBeginPassInfo = {};
	renderBeginPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderBeginPassInfo.renderPass = renderPass;
	renderBeginPassInfo.framebuffer = framebuffers[currentFrame];
	renderBeginPassInfo.renderArea.offset = { 0, 0 };
	renderBeginPassInfo.renderArea.extent = surfaceExtent;
	renderBeginPassInfo.clearValueCount = 2;
	renderBeginPassInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(cmdBuffers[currentFrame], &renderBeginPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VKRenderer::EndDefaultRenderPass()
{
	vkCmdEndRenderPass(cmdBuffers[currentFrame]);
}

void VKRenderer::EndCmdRecording()
{
	if (vkEndCommandBuffer(cmdBuffers[currentFrame]) != VK_SUCCESS)
	{
		std::cout << "Failed to record command buffer!\n";
	}
}

bool VKRenderer::CreateRenderPass(const VKBase& base, VkRenderPass& renderPass, VkFormat depthFormat)
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = base.GetSurfaceFormat().format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Sub passes
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;										// Which attachment to reference by its index in the attachment descriptions array. We only have one so its 0
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;	// Which layout we would like the attachment to have during a subpass that uses this reference. Vulkan automatically transition the attachment
																			// to this layout when the subpass is started. We intend to use the attachment as a color buffer and VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
																			// layout will give us the best performance.
	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &colorAttachmentRef;
	subpassDesc.pDepthStencilAttachment = &depthAttachmentRef;

	// Subpasses in a render pass automatically take care of image layout transitions. These transitions are controlled by subpass dependencies
	VkSubpassDependency dependencies[2] = {};
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;

	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].srcAccessMask = 0;

	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;

	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].dstAccessMask = 0;


	VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDesc;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies;

	if (vkCreateRenderPass(base.GetDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		std::cout << "Failed to create render pass\n";
		return false;
	}
	std::cout << "Created render pass\n";

	return true;
}

bool VKRenderer::CreateFramebuffers(const VKBase& base, VkRenderPass renderPass, std::vector<VkFramebuffer>& framebuffers, VkImageView depthImageView)
{
	framebuffers.resize(base.GetSwapchainImageCount());

	const std::vector<VkImage> swapChainImageViews = base.GetSwapchainImageViews();

	for (size_t i = 0; i < base.GetSwapchainImageCount(); i++)
	{
		VkImageView attachments[] = { swapChainImageViews[i], depthImageView };

		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = base.GetSurfaceExtent().width;
		framebufferInfo.height = base.GetSurfaceExtent().height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(base.GetDevice(), &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
		{
			std::cout << "Failed to create swapchain framebuffers\n";
			return false;
		}
	}
	std::cout << "Created framebuffers\n";

	return true;
}
