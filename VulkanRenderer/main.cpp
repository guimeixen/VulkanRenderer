#include "VKBase.h"
#include "VKShader.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <iostream>
#include <string>
#include <set>
#include <array>
#include <chrono>

int main()
{
	const int MAX_FRAMES_IN_FLIGHT = 2;
	const unsigned int width = 640;
	const unsigned int height = 480;
	
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

	VKBase base;
	if (!base.Init(window, true, width, height))
	{
		glfwTerminate();
		return 1;
	}

	VkDevice device = base.GetDevice();
	VkExtent2D surfaceExtent = base.GetSurfaceExtent();
	VkSurfaceFormatKHR surfaceFormat = base.GetSurfaceFormat();

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
	framebuffers.resize(base.GetSwapchainImageCount());

	const std::vector<VkImage> swapChainImageViews = base.GetSwapchainImageViews();

	for (size_t i = 0; i < base.GetSwapchainImageCount(); i++)
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


	struct Vertex
	{
		glm::vec2 pos;
		glm::vec3 color;
	};

	const std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
		{{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
		{{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		{{-0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}},
	};

	const std::vector<unsigned short> indices = { 
		0, 1, 2, 2, 3, 0
	};

	VKBuffer vertexStagingBuffer, indexStagingBuffer;
	vertexStagingBuffer.Create(device, base.GetPhysicalDeviceMemoryProperties(), sizeof(Vertex) * vertices.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	indexStagingBuffer.Create(device, base.GetPhysicalDeviceMemoryProperties(), sizeof(unsigned short)* indices.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	unsigned int vertexSize = vertexStagingBuffer.GetSize();

	void* data;
	vkMapMemory(device, vertexStagingBuffer.GetBufferMemory(), 0, vertexSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)vertexSize);
	vkUnmapMemory(device, vertexStagingBuffer.GetBufferMemory());

	unsigned int indexSize = indexStagingBuffer.GetSize();
	vkMapMemory(device, indexStagingBuffer.GetBufferMemory(), 0, indexSize, 0, &data);
	memcpy(data, indices.data(), (size_t)indexSize);
	vkUnmapMemory(device, indexStagingBuffer.GetBufferMemory());


	VKBuffer vb, ib;
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

	VkDescriptorSetLayoutBinding setlayoutBinding = {};
	setlayoutBinding.binding = 0;
	setlayoutBinding.descriptorCount = 1;
	setlayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	setlayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo setLayoutInfo = {};
	setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutInfo.bindingCount = 1;
	setLayoutInfo.pBindings = &setlayoutBinding;

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

	VkDescriptorPoolSize descPoolSize = {};
	descPoolSize.descriptorCount = base.GetSwapchainImageCount();
	descPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.maxSets = base.GetSwapchainImageCount();
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = &descPoolSize;

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

	std::vector<VkDescriptorSet> descriptorSets;

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

		VkWriteDescriptorSet descriptorWrite = {};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = descriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	}

	VkVertexInputBindingDescription bindingDesc = {};
	bindingDesc.binding = 0;
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc.stride = sizeof(Vertex);

	VkVertexInputAttributeDescription attribDesc[2] = {};
	attribDesc[0].binding = 0;
	attribDesc[0].format = VK_FORMAT_R32G32_SFLOAT;
	attribDesc[0].location = 0;
	attribDesc[0].offset = 0;

	attribDesc[1].binding = 0;
	attribDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDesc[1].location = 1;
	attribDesc[1].offset = offsetof(Vertex, color);

	VKShader shader;
	shader.LoadShader(device, "Data/Shaders/shader_vert_ubo.spv", "Data/Shaders/shader_buffer_frag.spv");

	VkPipelineShaderStageCreateInfo shaderStages[] = { shader.GetVertexStageInfo(), shader.GetFragmentStageInfo() };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
	vertexInputInfo.vertexAttributeDescriptionCount = 2;
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

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineLayout pipelineLayout;

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
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VkPipeline pipeline;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		std::cout << "Failed to create graphics pipeline\n";
		return 1;
	}
	std::cout << "Create pipeline\n";
	


	cmdBuffers.resize(framebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = base.GetCommandPool();
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

		VkClearValue clearColor = { 0.3f, 0.3f, 0.3f, 1.0f };

		VkRenderPassBeginInfo renderBeginPassInfo = {};
		renderBeginPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderBeginPassInfo.renderPass = renderPass;
		renderBeginPassInfo.framebuffer = framebuffers[i];
		renderBeginPassInfo.renderArea.offset = { 0, 0 };
		renderBeginPassInfo.renderArea.extent = surfaceExtent;
		renderBeginPassInfo.clearValueCount = 1;
		renderBeginPassInfo.pClearValues = &clearColor;

		VkBuffer vertexBuffers[] = { vb.GetBuffer() };
		VkDeviceSize offsets[] = {0};

		vkCmdBeginRenderPass(cmdBuffers[i], &renderBeginPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdBindVertexBuffers(cmdBuffers[i], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuffers[i], ib.GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);
		vkCmdDrawIndexed(cmdBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
		vkCmdEndRenderPass(cmdBuffers[i]);

		if (vkEndCommandBuffer(cmdBuffers[i]) != VK_SUCCESS)
		{
			std::cout << "Failed to record command buffer!\n";
			return 1;
		}
	}

	VkQueue graphicsQueue = base.GetGraphicsQueue();
	VkQueue presentQueue = base.GetPresentQueue();
	size_t currentFrame = 0;

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		vkWaitForFences(device, 1, &frameFences[currentFrame], VK_TRUE, UINT64_MAX);
		
		uint32_t imageIndex;
		vkAcquireNextImageKHR(device, base.GetSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		CameraUBO ubo = {};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
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

		VkSwapchainKHR swapChains[] = { base.GetSwapchain() };

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

	vkDestroyDescriptorSetLayout(device, setLayout, nullptr);

	for (uint32_t i = 0; i < base.GetSwapchainImageCount(); i++)
	{
		ubos[i].Dispose(device);
	}

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