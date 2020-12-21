#include "VKBase.h"
#include "VKShader.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "stb_image.h"

#include <iostream>
#include <string>
#include <set>
#include <array>
#include <chrono>

unsigned int width = 640;
unsigned int height = 480;
VkPipeline pipeline;
VkPipelineLayout pipelineLayout;
VKBuffer ib, vb;
std::vector<VkDescriptorSet> descriptorSets;

const std::vector<unsigned short> indices = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4
};

static void FramebufferResizeCallback(GLFWwindow *window, int newWidth, int newHeight)
{
	width = newWidth;
	height = newHeight;
}

bool CreateRenderPass(const VKBase &base, VkRenderPass &renderPass, VkFormat depthFormat)
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

bool CreateFramebuffers(const VKBase &base, VkRenderPass renderPass, std::vector<VkFramebuffer> &framebuffers, VkImageView depthImageView)
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

bool CreateCommandBuffers(const VKBase &base, VkRenderPass renderPass, VkExtent2D surfaceExtent, const std::vector<VkFramebuffer> &framebuffers, std::vector<VkCommandBuffer> &cmdBuffers)
{
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

	for (size_t i = 0; i < cmdBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(cmdBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			std::cout << "Failed to begin recording command buffer!\n";
			return false;
		}

		VkClearValue clearValues[2] = {};
		clearValues[0].color = { 0.3f, 0.3f, 0.3f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderBeginPassInfo = {};
		renderBeginPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderBeginPassInfo.renderPass = renderPass;
		renderBeginPassInfo.framebuffer = framebuffers[i];
		renderBeginPassInfo.renderArea.offset = { 0, 0 };
		renderBeginPassInfo.renderArea.extent = surfaceExtent;
		renderBeginPassInfo.clearValueCount = 2;
		renderBeginPassInfo.pClearValues = clearValues;

		VkBuffer vertexBuffers[] = { vb.GetBuffer() };
		VkDeviceSize offsets[] = { 0 };

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)surfaceExtent.width;
		viewport.height = (float)surfaceExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		vkCmdBeginRenderPass(cmdBuffers[i], &renderBeginPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdSetViewport(cmdBuffers[i], 0, 1, &viewport);
		vkCmdBindPipeline(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdBindVertexBuffers(cmdBuffers[i], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuffers[i], ib.GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
		vkCmdDrawIndexed(cmdBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
		vkCmdEndRenderPass(cmdBuffers[i]);

		if (vkEndCommandBuffer(cmdBuffers[i]) != VK_SUCCESS)
		{
			std::cout << "Failed to record command buffer!\n";
			return false;
		}
	}

	return true;
}

int main()
{
	const int MAX_FRAMES_IN_FLIGHT = 2;
		
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
	GLFWwindow *window = glfwCreateWindow((int)width, (int)height, "VulkanRenderer", nullptr, nullptr);

	glfwSetWindowPos(window, 400, 100);
	glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);

	VKBase base;
	if (!base.Init(window, true, width, height))
	{
		glfwTerminate();
		return 1;
	}

	VkDevice device = base.GetDevice();
	VkExtent2D surfaceExtent = base.GetSurfaceExtent();
	VkSurfaceFormatKHR surfaceFormat = base.GetSurfaceFormat();

	// Depth texture

	VkImage depthImage;
	VkImageView depthImageView;
	VkDeviceMemory depthImageMemory;

	VkFormat depthFormat = vkutils::FindSupportedFormat(base.GetPhysicalDevice(), { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	VkImageCreateInfo depthImageInfo = {};
	depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImageInfo.arrayLayers = 1;
	depthImageInfo.extent.width = static_cast<uint32_t>(base.GetSurfaceExtent().width);
	depthImageInfo.extent.height = static_cast<uint32_t>(base.GetSurfaceExtent().height);
	depthImageInfo.extent.depth = 1;
	depthImageInfo.format = depthFormat;
	depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthImageInfo.mipLevels = 1;
	depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthImageInfo.flags = 0;

	if (vkCreateImage(device, &depthImageInfo, nullptr, &depthImage) != VK_SUCCESS)
	{
		std::cout << "Failed to create depth image\n";
		return 1;
	}

	VkMemoryRequirements depthImageMemReqs;
	vkGetImageMemoryRequirements(device, depthImage, &depthImageMemReqs);

	VkMemoryAllocateInfo imgAllocInfo = {};
	imgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	imgAllocInfo.memoryTypeIndex = vkutils::FindMemoryType(base.GetPhysicalDeviceMemoryProperties(), depthImageMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imgAllocInfo.allocationSize = depthImageMemReqs.size;

	if (vkAllocateMemory(device, &imgAllocInfo, nullptr, &depthImageMemory) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate depth image memory\n";
		return 1;
	}

	vkBindImageMemory(device, depthImage, depthImageMemory, 0);

	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = depthImage;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = depthFormat;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device, &imageViewInfo, nullptr, &depthImageView) != VK_SUCCESS)
	{
		std::cout << "Failed to create depth image view\n";
		return 1;
	}




	if (!CreateRenderPass(base, renderPass, depthFormat))
		return 1;
	if (!CreateFramebuffers(base, renderPass, framebuffers, depthImageView))
		return 1;

	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	frameFences.resize(MAX_FRAMES_IN_FLIGHT);
	imagesInFlight.resize(base.GetSwapchainImageCount(), VK_NULL_HANDLE);

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


	// Load Texture

	unsigned char* pixels = nullptr;
	int width, height, channels;
	pixels = stbi_load("Data/Textures/front.png", &width, &height, &channels, STBI_rgb_alpha);

	unsigned int textureSize = (unsigned int)(width * height * 4);

	if (!pixels)
	{
		std::cout << "Failed to load texture\n";
		return 1;
	}

	VKBuffer texStagingBuffer;

	texStagingBuffer.Create(device, base.GetPhysicalDeviceMemoryProperties(), textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	
	void* data;
	vkMapMemory(device, texStagingBuffer.GetBufferMemory(), 0, static_cast<VkDeviceSize>(texStagingBuffer.GetSize()), 0, &data);
	memcpy(data, pixels, static_cast<size_t>(texStagingBuffer.GetSize()));
	vkUnmapMemory(device, texStagingBuffer.GetBufferMemory());

	VkImage texImage;
	VkDeviceMemory texMemory;

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.arrayLayers = 1;
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.mipLevels = 1;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	if (vkCreateImage(device, &imageInfo, nullptr, &texImage) != VK_SUCCESS)
	{
		std::cout << "Failed to create image for texture\n";
		return 1;
	}

	VkMemoryRequirements imageMemReqs;
	vkGetImageMemoryRequirements(device, texImage, &imageMemReqs);

	imgAllocInfo = {};
	imgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	imgAllocInfo.memoryTypeIndex = vkutils::FindMemoryType(base.GetPhysicalDeviceMemoryProperties(), imageMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imgAllocInfo.allocationSize = imageMemReqs.size;

	if (vkAllocateMemory(device, &imgAllocInfo, nullptr, &texMemory) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate image memory\n";
		return 1;
	}

	vkBindImageMemory(device, texImage, texMemory, 0);

	base.TransitionImageLayout(texImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	base.CopyBufferToImage(texStagingBuffer, texImage, width, height);
	base.TransitionImageLayout(texImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	texStagingBuffer.Dispose(device);

	VkImageView imageView;

	imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = texImage;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device, &imageViewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		std::cout << "Failed to create texture image view\n";
		return 1;
	}

	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	VkSampler sampler;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
	{
		std::cout << "Failed to create sampler\n";
		return 1;
	}


	struct Vertex
	{
		glm::vec3 pos;
		glm::vec2 uv;
		glm::vec3 color;
	};

	const std::vector<Vertex> vertices = {
		/*{{-0.5f, -0.5f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}},*/
		{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}},

		{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f}}
	};

	

	VKBuffer vertexStagingBuffer, indexStagingBuffer;
	vertexStagingBuffer.Create(device, base.GetPhysicalDeviceMemoryProperties(), sizeof(Vertex) * vertices.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	indexStagingBuffer.Create(device, base.GetPhysicalDeviceMemoryProperties(), sizeof(unsigned short)* indices.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	unsigned int vertexSize = vertexStagingBuffer.GetSize();

	vkMapMemory(device, vertexStagingBuffer.GetBufferMemory(), 0, vertexSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)vertexSize);
	vkUnmapMemory(device, vertexStagingBuffer.GetBufferMemory());

	unsigned int indexSize = indexStagingBuffer.GetSize();
	vkMapMemory(device, indexStagingBuffer.GetBufferMemory(), 0, indexSize, 0, &data);
	memcpy(data, indices.data(), (size_t)indexSize);
	vkUnmapMemory(device, indexStagingBuffer.GetBufferMemory());


	vb.Create(device, base.GetPhysicalDeviceMemoryProperties(), sizeof(Vertex) * vertices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ib.Create(device, base.GetPhysicalDeviceMemoryProperties(), sizeof(unsigned short)* indices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	base.CopyBuffer(vertexStagingBuffer, vb, vertexSize);
	base.CopyBuffer(indexStagingBuffer, ib, indexSize);

	vertexStagingBuffer.Dispose(device);
	indexStagingBuffer.Dispose(device);


	struct CameraUBO
	{
		glm::mat4 proj;
		glm::mat4 view;
		glm::mat4 model;
	};

	VkDescriptorSetLayoutBinding ubolayoutBinding = {};
	ubolayoutBinding.binding = 0;
	ubolayoutBinding.descriptorCount = 1;
	ubolayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubolayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutBinding textureLayoutBinding = {};
	textureLayoutBinding.binding = 1;
	textureLayoutBinding.descriptorCount = 1;
	textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding layoutBindings[] = { ubolayoutBinding, textureLayoutBinding };

	VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
	setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutInfo.bindingCount = 2;
	setLayoutInfo.pBindings = layoutBindings;

	VkDescriptorSetLayout setLayout;

	if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &setLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor set layout\n";
		return 1;
	}

	std::vector<VKBuffer> ubos(base.GetSwapchainImageCount());

	for (uint32_t i = 0; i < base.GetSwapchainImageCount(); i++)
	{
		ubos[i].Create(device, base.GetPhysicalDeviceMemoryProperties(), sizeof(CameraUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	}

	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].descriptorCount = base.GetSwapchainImageCount();
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	poolSizes[1].descriptorCount = base.GetSwapchainImageCount();
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.maxSets = base.GetSwapchainImageCount();
	descPoolInfo.poolSizeCount = 2;
	descPoolInfo.pPoolSizes = poolSizes;

	VkDescriptorPool descriptorPool;

	if (vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor pool\n";
		return 1;
	}
	std::cout << "Created descriptor pool\n";

	std::vector<VkDescriptorSetLayout> layouts(base.GetSwapchainImageCount(), setLayout);

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = static_cast<uint32_t>(base.GetSwapchainImageCount());
	setAllocInfo.pSetLayouts = layouts.data();


	descriptorSets.resize(base.GetSwapchainImageCount());
	if (vkAllocateDescriptorSets(device, &setAllocInfo, descriptorSets.data()) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}
	std::cout << "Allocated descriptor sets\n";

	for (size_t i = 0; i < base.GetSwapchainImageCount(); i++)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = ubos[i].GetBuffer();
		bufferInfo.offset = 0;
		bufferInfo.range = VK_WHOLE_SIZE;

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = imageView;
		imageInfo.sampler = sampler;

		VkWriteDescriptorSet descriptorWrites[2] = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(device, 2, descriptorWrites, 0, nullptr);
	}

	VkVertexInputBindingDescription bindingDesc = {};
	bindingDesc.binding = 0;
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc.stride = sizeof(Vertex);

	VkVertexInputAttributeDescription attribDesc[3] = {};
	attribDesc[0].binding = 0;
	attribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDesc[0].location = 0;
	attribDesc[0].offset = 0;

	attribDesc[1].binding = 0;
	attribDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
	attribDesc[1].location = 1;
	attribDesc[1].offset = offsetof(Vertex, uv);

	attribDesc[2].binding = 0;
	attribDesc[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDesc[2].location = 2;
	attribDesc[2].offset = offsetof(Vertex, color);

	VKShader shader;
	shader.LoadShader(device, "Data/Shaders/shader_tex_vert.spv", "Data/Shaders/shader_tex_frag.spv");

	VkPipelineShaderStageCreateInfo shaderStages[] = { shader.GetVertexStageInfo(), shader.GetFragmentStageInfo() };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
	vertexInputInfo.vertexAttributeDescriptionCount = 3;
	vertexInputInfo.pVertexAttributeDescriptions = attribDesc;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)surfaceExtent.width;
	viewport.height = (float)surfaceExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = surfaceExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilState.depthBoundsTestEnable = VK_FALSE;
	depthStencilState.stencilTestEnable = VK_FALSE;

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		//VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 1;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &setLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create pipeline layout\n";
		return 1;
	}
	std::cout << "Create pipeline layout\n";

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencilState;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		std::cout << "Failed to create graphics pipeline\n";
		return 1;
	}
	std::cout << "Create pipeline\n";
	

	if (!CreateCommandBuffers(base, renderPass, surfaceExtent, framebuffers, cmdBuffers))
		return 1;
	

	VkQueue graphicsQueue = base.GetGraphicsQueue();
	VkQueue presentQueue = base.GetPresentQueue();
	size_t currentFrame = 0;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		vkWaitForFences(device, 1, &frameFences[currentFrame], VK_TRUE, UINT64_MAX);
		
		uint32_t imageIndex;
		VkResult res = vkAcquireNextImageKHR(device, base.GetSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

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
			CreateRenderPass(base, renderPass, depthFormat);
			CreateFramebuffers(base, renderPass, framebuffers, depthImageView);
			CreateCommandBuffers(base, renderPass, base.GetSurfaceExtent(), framebuffers, cmdBuffers);

			// We can't present so go to the next iteration
			continue;
		}

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		CameraUBO ubo = {};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(20.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		//ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		ubo.view = glm::lookAt(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.proj = glm::perspective(glm::radians(85.0f), (float)width / height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		void* data;
		vkMapMemory(device, ubos[imageIndex].GetBufferMemory(), 0, sizeof(CameraUBO), 0, &data);
		memcpy(data, &ubo, sizeof(CameraUBO));
		vkUnmapMemory(device, ubos[imageIndex].GetBufferMemory());


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
		submitInfo.pCommandBuffers = &cmdBuffers[imageIndex];
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &renderFinishedSemaphores[currentFrame];

		vkResetFences(device, 1, &frameFences[currentFrame]);

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameFences[currentFrame]) != VK_SUCCESS)
		{
			std::cout << "Failed to submit draw command buffer!\n";
			return 1;
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
			CreateRenderPass(base, renderPass, depthFormat);
			CreateFramebuffers(base, renderPass, framebuffers, depthImageView);
			CreateCommandBuffers(base, renderPass, base.GetSurfaceExtent(), framebuffers, cmdBuffers);
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	vkDeviceWaitIdle(device);

	vkDestroyDescriptorSetLayout(device, setLayout, nullptr);

	for (uint32_t i = 0; i < base.GetSwapchainImageCount(); i++)
	{
		ubos[i].Dispose(device);
	}

	vkDestroyImage(device, depthImage, nullptr);
	vkFreeMemory(device, depthImageMemory, nullptr);
	vkDestroyImageView(device, depthImageView, nullptr);

	vkDestroyImage(device, texImage, nullptr);
	vkDestroyImageView(device, imageView, nullptr);
	vkDestroySampler(device, sampler, nullptr);
	vkFreeMemory(device, texMemory, nullptr);

	vb.Dispose(device);
	ib.Dispose(device);

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);

	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

	shader.Dispose(device);

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
	
	base.Dispose();

	glfwTerminate();

	return 0;
}