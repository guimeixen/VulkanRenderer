#include "VKFramebuffer.h"

#include <iostream>
#include <vector>

VKFramebuffer::VKFramebuffer()
{
    framebuffer = VK_NULL_HANDLE;
    renderPass = VK_NULL_HANDLE;
}

bool VKFramebuffer::Create(const VKBase &base, const FramebufferParams& params, unsigned int width, unsigned int height)
{
	this->width = width;
	this->height = height;

	std::vector<VkAttachmentDescription> attachmentDescriptions;

	VkAttachmentReference colorAttachRef = {};
	colorAttachRef.attachment = 0;
	colorAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachRef = {};
	depthAttachRef.attachment = static_cast<uint32_t>(params.colorTexturesParams.size());		// Depth attachment comes after the color textures
	depthAttachRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

	if (params.createColorTexture)
	{
		if (params.colorTexturesParams.size() == 0)
		{
			std::cout << "Framebuffer has createColorTexture true but no texture params provided\n";
			return false;
		}

		VKTexture2D texture;

		if (!texture.CreateColorTexture(base, params.colorTexturesParams[0], width, height))
			return false;

		colorTextures.push_back(texture);

		// Color attachment
		VkAttachmentDescription desc = {};
		desc.format = params.colorTexturesParams[0].format;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		desc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		attachmentDescriptions.push_back(desc);

		subpassDesc.colorAttachmentCount = 1;
		subpassDesc.pColorAttachments = &colorAttachRef;
	}

	if (params.createDepthTexture)
	{
		if (!depthTexture.CreateDepthTexture(base, params.depthTexturesParams, width, height, params.sampleDepth))
			return false;

		// Depth attachment
		VkAttachmentDescription desc = {};
		desc.format = params.depthTexturesParams.format;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (params.sampleDepth)
		{
			desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		}
		else
		{
			desc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		attachmentDescriptions.push_back(desc);

		subpassDesc.pDepthStencilAttachment = &depthAttachRef;
	}

	// Use subpass dependencies for layout transitions
	VkSubpassDependency dependencies[2] = {};

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	
	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	
	if (params.colorTexturesParams.size() > 0)
	{
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	}
	else if (params.sampleDepth)
	{
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	}


	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
	renderPassInfo.pAttachments = attachmentDescriptions.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDesc;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = dependencies;

	VkDevice device = base.GetDevice();

	if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
	{
		std::cout << "Failed to create offscreen render pass!\n";
		return false;
	}

	std::vector<VkImageView> attachments;

	for (size_t i = 0; i < colorTextures.size(); i++)
	{
		attachments.push_back(colorTextures[i].GetImageView());
	}

	attachments.push_back(depthTexture.GetImageView());

	VkFramebufferCreateInfo fbInfo = {};
	fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbInfo.height = static_cast<uint32_t>(height);
	fbInfo.width = static_cast<uint32_t>(width);
	fbInfo.layers = 1;
	fbInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	fbInfo.pAttachments = attachments.data();
	fbInfo.renderPass = renderPass;

	if (vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffer) != VK_SUCCESS)
	{
		std::cout << "Failed to create offscreen framebuffer!\n";
		return 1;
	}

	return true;
}

void VKFramebuffer::Dispose(VkDevice device)
{
	for (size_t i = 0; i < colorTextures.size(); i++)
	{
		colorTextures[i].Dispose(device);
	}
	depthTexture.Dispose(device);

	vkDestroyRenderPass(device, renderPass, nullptr);
	vkDestroyFramebuffer(device, framebuffer, nullptr);
}
