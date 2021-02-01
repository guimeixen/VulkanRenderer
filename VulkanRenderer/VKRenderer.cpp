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
	if (!base.Init(window, width, height, true))
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
	allocInfo.commandPool = base.GetGraphicsCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)cmdBuffers.size();

	if (vkAllocateCommandBuffers(base.GetDevice(), &allocInfo, cmdBuffers.data()) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate command buffers!\n";
		return false;
	}

	// Descriptor pool

	VkDescriptorPoolSize poolSizes[4] = {};
	poolSizes[0].descriptorCount = 1;
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

	poolSizes[1].descriptorCount = 10;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	poolSizes[2].descriptorCount = 1;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	poolSizes[3].descriptorCount = 1;
	poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.maxSets = 8;
	descPoolInfo.poolSizeCount = 4;
	descPoolInfo.pPoolSizes = poolSizes;

	if (vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor pool\n";
		return false;
	}
	std::cout << "Created descriptor pool\n";

	// Global Buffers

	VkDescriptorSetLayoutBinding cameraUboBinding = {};
	cameraUboBinding.binding = 0;
	cameraUboBinding.descriptorCount = 1;
	cameraUboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	cameraUboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding instanceBufferBinding = {};
	instanceBufferBinding.binding = 1;
	instanceBufferBinding.descriptorCount = 1;
	instanceBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	instanceBufferBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding globalBuffersSetBindings[] = { cameraUboBinding, instanceBufferBinding };

	VkDescriptorSetLayoutCreateInfo globalBuffersSetLayoutInfo = {};
	globalBuffersSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	globalBuffersSetLayoutInfo.bindingCount = 2;
	globalBuffersSetLayoutInfo.pBindings = globalBuffersSetBindings;

	if (vkCreateDescriptorSetLayout(device, &globalBuffersSetLayoutInfo, nullptr, &globalBuffersSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create global buffers descriptor set layout\n";
		return false;
	}

	// Global Textures

	VkDescriptorSetLayoutBinding shadowMapLayoutBinding = {};
	shadowMapLayoutBinding.binding = 0;
	shadowMapLayoutBinding.descriptorCount = 1;
	shadowMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding storageImageLayoutBinding = {};
	storageImageLayoutBinding.binding = 1;
	storageImageLayoutBinding.descriptorCount = 1;
	storageImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	storageImageLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding texturesSetLayoutBindings[] = { shadowMapLayoutBinding, storageImageLayoutBinding };

	VkDescriptorSetLayoutCreateInfo globalTexturesSetLayoutInfo = {};
	globalTexturesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	globalTexturesSetLayoutInfo.bindingCount = 2;
	globalTexturesSetLayoutInfo.pBindings = texturesSetLayoutBindings;

	if (vkCreateDescriptorSetLayout(device, &globalTexturesSetLayoutInfo, nullptr, &globalTexturesSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create textures descriptor set layout\n";
		return false;
	}

	// User Textures

	VkDescriptorSetLayoutBinding userTextureLayoutBinding = {};
	userTextureLayoutBinding.binding = 0;
	userTextureLayoutBinding.descriptorCount = 1;
	userTextureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	userTextureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo userTexturesSetLayoutInfo = {};
	userTexturesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	userTexturesSetLayoutInfo.bindingCount = 1;
	userTexturesSetLayoutInfo.pBindings = &userTextureLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &userTexturesSetLayoutInfo, nullptr, &userTexturesSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create textures descriptor set layout\n";
		return false;
	}

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(unsigned int);

	// Create pipeline layout
	VkDescriptorSetLayout setLayouts[] = { globalBuffersSetLayout, globalTexturesSetLayout, userTexturesSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 3;
	pipelineLayoutInfo.pSetLayouts = setLayouts;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create pipeline layout\n";
		return false;
	}
	std::cout << "Create pipeline layout\n";


	// Allocate the descriptor sets
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &globalBuffersSetLayout;

	if (vkAllocateDescriptorSets(device, &setAllocInfo, &globalBuffersSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &globalTexturesSetLayout;
	if (vkAllocateDescriptorSets(device, &setAllocInfo, &globalTexturesSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	std::cout << "Allocated descriptor sets\n";

	return true;
}

void VKRenderer::Dispose()
{
	VkDevice device = base.GetDevice();
	depthTexture.Dispose(device);

	vkDestroyDescriptorSetLayout(device, globalBuffersSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, globalTexturesSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, userTexturesSetLayout, nullptr);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

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

void VKRenderer::AcquireNextImage()
{
	VkDevice device = base.GetDevice();

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

		vkFreeCommandBuffers(device, base.GetGraphicsCommandPool(), static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());

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
}

void VKRenderer::Present(VkSemaphore graphicsSemaphore, VkSemaphore computeSemaphore)
{
	VkDevice device = base.GetDevice();
	VkQueue graphicsQueue = base.GetGraphicsQueue();
	VkQueue presentQueue = base.GetPresentQueue();

	// The image is now in use by this frame
	imagesInFlight[imageIndex] = frameFences[currentFrame];

	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
	//VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame], computeSemaphore };
	//VkSemaphore waitSemaphores[] = { computeSemaphore };

	VkSemaphore signalSemaphores[] = { graphicsSemaphore, renderFinishedSemaphores[currentFrame] };

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 2;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffers[currentFrame];
	submitInfo.signalSemaphoreCount = 2;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(device, 1, &frameFences[currentFrame]);

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFences[currentFrame]) != VK_SUCCESS)
	{
		std::cout << "Failed to submit draw command buffer!\n";
	}

	VkSwapchainKHR swapChain[] = { base.GetSwapchain() };

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinishedSemaphores[currentFrame];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChain;
	presentInfo.pImageIndices = &imageIndex;

	VkResult res = vkQueuePresentKHR(presentQueue, &presentInfo);

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

		vkFreeCommandBuffers(device, base.GetGraphicsCommandPool(), static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());

		base.RecreateSwapchain(width, height);
		CreateRenderPass(base, renderPass, depthTexture.GetFormat());
		CreateFramebuffers(base, renderPass, framebuffers, depthTexture.GetImageView());
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VKRenderer::CreateMipMaps(VkCommandBuffer cmdBuffer, const VKTexture2D& texture)
{
	for (unsigned int i = 1; i < texture.GetNumMipLevels(); i++)
	{
		VkImageBlit blit = {};

		// Source
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.layerCount = 1;
		blit.srcSubresource.mipLevel = static_cast<uint32_t>(i - 1);
		blit.srcOffsets[1].x = static_cast<int32_t>(texture.GetWidth() >> (i - 1));
		blit.srcOffsets[1].y = static_cast<int32_t>(texture.GetHeight() >> (i - 1));
		blit.srcOffsets[1].z = 1;

		// Destination
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.layerCount = 1;
		blit.dstSubresource.mipLevel = static_cast<uint32_t>(i);
		blit.dstOffsets[1].x = static_cast<int32_t>(texture.GetWidth() >> i);
		blit.dstOffsets[1].y = static_cast<int32_t>(texture.GetHeight() >> i);
		blit.dstOffsets[1].z = 1;

		// To account for non square texture (eg 512x1024), when one of the sides reaches zero  4x8,2x4,1x2,0x1!! so we need to correct to 1x1
		if (blit.dstOffsets[1].x == 0)
			blit.dstOffsets[1].x = 1;
		if (blit.dstOffsets[1].y == 0)
			blit.dstOffsets[1].y = 1;

		// Preprare this mip. Transition this mip level to TRANSFER_DST
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.image = texture.GetImage();
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.baseMipLevel = i;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		vkCmdBlitImage(cmdBuffer, texture.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, texture.GetImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		// Prepare the current mip as the source for the next mip
		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	// All mips are in TRANSFER_SRC, so transition them all to SHADER_READ
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.image = texture.GetImage();
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = texture.GetNumMipLevels();
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkDescriptorSet VKRenderer::AllocateUserTextureDescriptorSet()
{
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &userTexturesSetLayout;

	VkDescriptorSet set;

	if (vkAllocateDescriptorSets(base.GetDevice(), &setAllocInfo, &set) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate descriptor set\n";
		return VK_NULL_HANDLE;
	}

	return set;
}

VkDescriptorSet VKRenderer::AllocateSetFromLayout(VkDescriptorSetLayout layout)
{
	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &layout;

	VkDescriptorSet set;

	if (vkAllocateDescriptorSets(base.GetDevice(), &setAllocInfo, &set) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate descriptor set\n";
		return VK_NULL_HANDLE;
	}

	return set;
}

void VKRenderer::UpdateGlobalBuffersSet(const VkDescriptorBufferInfo& info, uint32_t binding, VkDescriptorType descriptorType)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = globalBuffersSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorType = descriptorType;
	write.descriptorCount = 1;
	write.pBufferInfo = &info;

	vkUpdateDescriptorSets(base.GetDevice(), 1, &write, 0, nullptr);
}

void VKRenderer::UpdateGlobalTexturesSet(const VkDescriptorImageInfo& info, uint32_t binding, VkDescriptorType descriptorType)
{
	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = globalTexturesSet;
	write.dstBinding = binding;
	write.dstArrayElement = 0;
	write.descriptorType = descriptorType;
	write.descriptorCount = 1;
	write.pImageInfo = &info;

	vkUpdateDescriptorSets(base.GetDevice(), 1, &write, 0, nullptr);
}

VkCommandBuffer VKRenderer::CreateGraphicsCommandBuffer(bool beginRecord)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = base.GetGraphicsCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmdBuffer;

	if (vkAllocateCommandBuffers(base.GetDevice(), &allocInfo, &cmdBuffer) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate command buffer!\n";
		return VK_NULL_HANDLE;
	}

	if (beginRecord)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;

		if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS)
		{
			std::cout << "Failed to begin command buffer!\n";
			return VK_NULL_HANDLE;
		}
	}

	return cmdBuffer;
}

VkCommandBuffer VKRenderer::CreateComputeCommandBuffer(bool beginRecord)
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = base.GetComputeCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer cmdBuffer;

	if (vkAllocateCommandBuffers(base.GetDevice(), &allocInfo, &cmdBuffer) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate command buffer!\n";
		return VK_NULL_HANDLE;
	}

	if (beginRecord)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;

		if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS)
		{
			std::cout << "Failed to begin command buffer!\n";
			return VK_NULL_HANDLE;
		}
	}

	return cmdBuffer;
}

void VKRenderer::FreeGraphicsCommandBuffer(VkCommandBuffer cmdBuffer)
{
	vkFreeCommandBuffers(base.GetDevice(), base.GetGraphicsCommandPool(), 1, &cmdBuffer);
}

void VKRenderer::FreeComputeCommandBuffer(VkCommandBuffer cmdBuffer)
{
	vkFreeCommandBuffers(base.GetDevice(), base.GetComputeCommandPool(), 1, &cmdBuffer);
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

void VKRenderer::BeginRenderPass(VkCommandBuffer cmdBuffer, const VKFramebuffer& fb, uint32_t clearValueCount, const VkClearValue* clearValues)
{
	VkRenderPassBeginInfo renderBeginPassInfo = {};
	renderBeginPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderBeginPassInfo.renderPass = fb.GetRenderPass();
	renderBeginPassInfo.framebuffer = fb.GetFramebuffer();
	renderBeginPassInfo.renderArea.offset = { 0, 0 };
	renderBeginPassInfo.renderArea.extent = { fb.GetWidth(), fb.GetHeight() };
	renderBeginPassInfo.clearValueCount = clearValueCount;
	renderBeginPassInfo.pClearValues = clearValues;

	vkCmdBeginRenderPass(cmdBuffer, &renderBeginPassInfo, VK_SUBPASS_CONTENTS_INLINE);
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

void VKRenderer::AcquireImageBarrier(VkCommandBuffer cmdBuffer, const VKTexture2D& texture, int srcQueueFamilyIndex, int dstQueueFamilyIndex)
{
	VkImageMemoryBarrier acquireBarrier = {};
	acquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	acquireBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	acquireBarrier.srcAccessMask = 0;
	acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	acquireBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
	acquireBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
	acquireBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	acquireBarrier.subresourceRange.baseArrayLayer = 0;
	acquireBarrier.subresourceRange.baseMipLevel = 0;
	acquireBarrier.subresourceRange.layerCount = 1;
	acquireBarrier.subresourceRange.levelCount = 1;
	acquireBarrier.image = texture.GetImage();

	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &acquireBarrier);
}

void VKRenderer::ReleaseImageBarrier(VkCommandBuffer cmdBuffer, const VKTexture2D& texture, int srcQueueFamilyIndex, int dstQueueFamilyIndex)
{
	VkImageMemoryBarrier releaseBarrier = {};
	releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	releaseBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	releaseBarrier.dstAccessMask = 0;
	releaseBarrier.srcQueueFamilyIndex = srcQueueFamilyIndex;
	releaseBarrier.dstQueueFamilyIndex = dstQueueFamilyIndex;
	releaseBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	releaseBarrier.subresourceRange.baseArrayLayer = 0;
	releaseBarrier.subresourceRange.baseMipLevel = 0;
	releaseBarrier.subresourceRange.layerCount = 1;
	releaseBarrier.subresourceRange.levelCount = 1;
	releaseBarrier.image = texture.GetImage();

	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &releaseBarrier);
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
