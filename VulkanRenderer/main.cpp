#include "VKBase.h"
#include "VKShader.h"
#include "Model.h"
#include "VertexTypes.h"
#include "VKTexture2D.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "stb_image.h"

#include "assimp/scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"

#include <iostream>
#include <string>
#include <set>
#include <array>
#include <chrono>

unsigned int width = 640;
unsigned int height = 480;
const int MAX_FRAMES_IN_FLIGHT = 2;
VkPipeline pipeline;
VkPipelineLayout pipelineLayout;
//std::vector<VkDescriptorSet> descriptorSets;
VkDescriptorSet camUBOSet[MAX_FRAMES_IN_FLIGHT];

VkDescriptorSet modelSet;

VkPipeline skyboxPipeline;
VKBuffer skyboxVB;
//std::vector<VkDescriptorSet> skyboxSets;
VkDescriptorSet skyboxSet;

Model model;

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

bool CreateCommandBuffers(const VKBase &base, VkRenderPass renderPass, VkExtent2D surfaceExtent, const std::vector<VkFramebuffer> &framebuffers, unsigned int numIndices, std::vector<VkCommandBuffer> &cmdBuffers)
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

		VkBuffer vertexBuffers[] = { VK_NULL_HANDLE };
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

		vkCmdBindDescriptorSets(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &camUBOSet[i], 0, nullptr);

		vkCmdBindPipeline(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vertexBuffers[0] = model.GetVertexBuffer().GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffers[i], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuffers[i], model.GetIndexBuffer().GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		//vkCmdBindDescriptorSets(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
		vkCmdBindDescriptorSets(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &modelSet, 0, nullptr);
		vkCmdDrawIndexed(cmdBuffers[i], static_cast<uint32_t>(numIndices), 1, 0, 0, 0);

		vkCmdBindPipeline(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
		vertexBuffers[0] = skyboxVB.GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffers[i], 0, 1, vertexBuffers, offsets);
		vkCmdBindDescriptorSets(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &skyboxSet, 0, nullptr);
		vkCmdDraw(cmdBuffers[i], 36, 1, 0, 0);


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
	TextureParams depthTextureParams = {};
	depthTextureParams.format = vkutils::FindSupportedFormat(base.GetPhysicalDevice(), { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

	VKTexture2D depthTexture;
	depthTexture.CreateDepthTexture(base, depthTextureParams, base.GetSurfaceExtent().width, base.GetSurfaceExtent().height);


	if (!CreateRenderPass(base, renderPass, depthTexture.GetFormat()))
		return 1;
	if (!CreateFramebuffers(base, renderPass, framebuffers, depthTexture.GetImageView()))
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


	TextureParams textureParams = {};
	textureParams.format = VK_FORMAT_R8G8B8A8_SRGB;

	VKTexture2D texture;
	texture.LoadFromFile("Data/Models/trash_can_d.jpg", base, textureParams);

	model.Load("Data/Models/trash_can.obj", base);

	// Load cubemap
	std::vector<std::string> faces(6);
	faces[0] = "Data/Textures/left.png";
	faces[1] = "Data/Textures/right.png";
	faces[2] = "Data/Textures/up.png";
	faces[3] = "Data/Textures/down.png";
	faces[4] = "Data/Textures/front.png";
	faces[5] = "Data/Textures/back.png";

	VKTexture2D cubemap;
	cubemap.LoadCubemapFromFiles(faces, base, textureParams);

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

	VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
	setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutInfo.bindingCount = 1;
	setLayoutInfo.pBindings = &ubolayoutBinding;

	VkDescriptorSetLayout uboSetLayout;

	if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &uboSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor set layout\n";
		return 1;
	}

	VkDescriptorSetLayoutBinding textureLayoutBinding = {};
	textureLayoutBinding.binding = 0;
	textureLayoutBinding.descriptorCount = 1;
	textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	setLayoutInfo.bindingCount = 1;
	setLayoutInfo.pBindings = &textureLayoutBinding;

	VkDescriptorSetLayout texturesSetLayout;

	if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &texturesSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor set layout\n";
		return 1;
	}

	// We use a buffer with twice (or triple) the size and then offset into it so we don't have to create multiple buffers
	// Buffer create function takes care of aligment if the buffer is a ubo
	VKBuffer cameraUBO;
	cameraUBO.Create(&base, sizeof(CameraUBO) * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkDescriptorPoolSize poolSizes[2] = {};
	poolSizes[0].descriptorCount = MAX_FRAMES_IN_FLIGHT;
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	poolSizes[1].descriptorCount = 2;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.maxSets = MAX_FRAMES_IN_FLIGHT + 2;
	descPoolInfo.poolSizeCount = 2;
	descPoolInfo.pPoolSizes = poolSizes;

	VkDescriptorPool descriptorPool;

	if (vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor pool\n";
		return 1;
	}
	std::cout << "Created descriptor pool\n";


	VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT] = { uboSetLayout, uboSetLayout };

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
	setAllocInfo.pSetLayouts = layouts;


	/*descriptorSets.resize(base.GetSwapchainImageCount());
	if (vkAllocateDescriptorSets(device, &setAllocInfo, descriptorSets.data()) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}*/
	if (vkAllocateDescriptorSets(device, &setAllocInfo, camUBOSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &texturesSetLayout;
	if (vkAllocateDescriptorSets(device, &setAllocInfo, &modelSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	/*skyboxSets.resize(base.GetSwapchainImageCount());
	if (vkAllocateDescriptorSets(device, &setAllocInfo, skyboxSets.data()) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}*/

	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &texturesSetLayout;
	if (vkAllocateDescriptorSets(device, &setAllocInfo, &skyboxSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	std::cout << "Allocated descriptor sets\n";

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = cameraUBO.GetBuffer();
		bufferInfo.offset = i * (cameraUBO.GetSize() / base.GetSwapchainImageCount());
		bufferInfo.range = sizeof(CameraUBO);

		VkWriteDescriptorSet descriptorWrites = {};
		descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites.dstSet = camUBOSet[i];
		descriptorWrites.dstBinding = 0;
		descriptorWrites.dstArrayElement = 0;
		descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites.descriptorCount = 1;
		descriptorWrites.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);
	}

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = texture.GetImageView();
	imageInfo.sampler = texture.GetSampler();

	VkWriteDescriptorSet descriptorWrites = {};
	descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites.dstSet = modelSet;
	descriptorWrites.dstBinding = 0;
	descriptorWrites.dstArrayElement = 0;
	descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites.descriptorCount = 1;
	descriptorWrites.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);


	imageInfo.imageView = cubemap.GetImageView();
	imageInfo.sampler = cubemap.GetSampler();

	VkWriteDescriptorSet skyboxSetWrite = {};
	skyboxSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	skyboxSetWrite.dstSet = skyboxSet;
	skyboxSetWrite.dstBinding = 0;
	skyboxSetWrite.dstArrayElement = 0;
	skyboxSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	skyboxSetWrite.descriptorCount = 1;
	skyboxSetWrite.pImageInfo = &imageInfo;


	vkUpdateDescriptorSets(device, 1, &skyboxSetWrite, 0, nullptr);

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
	attribDesc[2].offset = offsetof(Vertex, normal);

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

	VkDescriptorSetLayout setLayouts[] = { uboSetLayout, texturesSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 2;
	pipelineLayoutInfo.pSetLayouts = setLayouts;
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



	float skyboxVertices[] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};

	VKBuffer vertexStagingBuffer;
	vertexStagingBuffer.Create(&base, sizeof(skyboxVertices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	unsigned int vertexSize = vertexStagingBuffer.GetSize();

	void* data;
	vkMapMemory(device, vertexStagingBuffer.GetBufferMemory(), 0, vertexSize, 0, &data);
	memcpy(data, skyboxVertices, (size_t)vertexSize);
	vkUnmapMemory(device, vertexStagingBuffer.GetBufferMemory());

	skyboxVB.Create(&base, sizeof(skyboxVertices), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	base.CopyBuffer(vertexStagingBuffer, skyboxVB, vertexSize);

	vertexStagingBuffer.Dispose(device);



	VKShader skyboxShader;
	skyboxShader.LoadShader(device, "Data/Shaders/skybox_vert.spv", "Data/Shaders/skybox_frag.spv");

	VkPipelineShaderStageCreateInfo skyboxShaderStages[] = { skyboxShader.GetVertexStageInfo(), skyboxShader.GetFragmentStageInfo() };

	bindingDesc.binding = 0;
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc.stride = sizeof(glm::vec3);

	attribDesc[0].binding = 0;
	attribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDesc[0].location = 0;
	attribDesc[0].offset = 0;

	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
	vertexInputInfo.vertexAttributeDescriptionCount = 1;
	vertexInputInfo.pVertexAttributeDescriptions = attribDesc;

	// Make sure to use LEQUAL in depth testing, because we set the skybox depth to 1 in the vertex shader
	// and otherwise wouldn't render if we kept using just LESS
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	VkGraphicsPipelineCreateInfo skyboxPipelineInfo = {};
	skyboxPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	skyboxPipelineInfo.stageCount = 2;
	skyboxPipelineInfo.pStages = skyboxShaderStages;
	skyboxPipelineInfo.pVertexInputState = &vertexInputInfo;
	skyboxPipelineInfo.pInputAssemblyState = &inputAssembly;
	skyboxPipelineInfo.pViewportState = &viewportState;
	skyboxPipelineInfo.pRasterizationState = &rasterizer;
	skyboxPipelineInfo.pMultisampleState = &multisampling;
	skyboxPipelineInfo.pDepthStencilState = &depthStencilState;
	skyboxPipelineInfo.pColorBlendState = &colorBlending;
	skyboxPipelineInfo.pDynamicState = &dynamicState;
	skyboxPipelineInfo.layout = pipelineLayout;
	skyboxPipelineInfo.renderPass = renderPass;
	skyboxPipelineInfo.subpass = 0;
	skyboxPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	skyboxPipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &skyboxPipelineInfo, nullptr, &skyboxPipeline) != VK_SUCCESS)
	{
		std::cout << "Failed to create skybox pipeline\n";
		return 1;
	}
	

	if (!CreateCommandBuffers(base, renderPass, surfaceExtent, framebuffers, model.GetIndexCount(), cmdBuffers))
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
			CreateRenderPass(base, renderPass, depthTexture.GetFormat());
			CreateFramebuffers(base, renderPass, framebuffers, depthTexture.GetImageView());
			CreateCommandBuffers(base, renderPass, base.GetSurfaceExtent(), framebuffers, model.GetIndexCount(), cmdBuffers);

			// We can't present so go to the next iteration
			continue;
		}

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		CameraUBO ubo = {};
		//ubo.model = glm::mat4(1.0f);
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		ubo.view = glm::lookAt(glm::vec3(0.0f, 0.5f, 1.5f), glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.proj = glm::perspective(glm::radians(85.0f), (float)width / height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		void* data;
		vkMapMemory(device, cameraUBO.GetBufferMemory(), imageIndex * (cameraUBO.GetSize() / base.GetSwapchainImageCount()), sizeof(CameraUBO), 0, &data);
		memcpy(data, &ubo, sizeof(CameraUBO));
		vkUnmapMemory(device, cameraUBO.GetBufferMemory());


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
			CreateRenderPass(base, renderPass, depthTexture.GetFormat());
			CreateFramebuffers(base, renderPass, framebuffers, depthTexture.GetImageView());
			CreateCommandBuffers(base, renderPass, base.GetSurfaceExtent(), framebuffers, model.GetIndexCount(), cmdBuffers);
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	vkDeviceWaitIdle(device);

	vkDestroyDescriptorSetLayout(device, uboSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, texturesSetLayout, nullptr);

	cameraUBO.Dispose(device);

	depthTexture.Dispose(device);
	cubemap.Dispose(device);
	texture.Dispose(device);
	model.Dispose(device);

	skyboxVB.Dispose(device);

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);

	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipeline(device, skyboxPipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

	shader.Dispose(device);
	skyboxShader.Dispose(device);

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