#include "Window.h"
#include "Input.h"
#include "Camera.h"
#include "VKRenderer.h"
#include "VKShader.h"
#include "Model.h"
#include "VertexTypes.h"
#include "VKTexture2D.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

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

int main()
{
	InputManager inputManager;
	Window window;
	window.Init(&inputManager, width, height);

	VKRenderer renderer;
	if (!renderer.Init(window.GetHandle(), width, height))
	{
		glfwTerminate();
		return 1;
	}

	VKBase& base = renderer.GetBase();

	VkDevice device = base.GetDevice();
	VkExtent2D surfaceExtent = base.GetSurfaceExtent();
	VkSurfaceFormatKHR surfaceFormat = base.GetSurfaceFormat();



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
	pipelineInfo.renderPass = renderer.GetDefaultRenderPass();
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
	skyboxPipelineInfo.renderPass = renderer.GetDefaultRenderPass();
	skyboxPipelineInfo.subpass = 0;
	skyboxPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	skyboxPipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &skyboxPipelineInfo, nullptr, &skyboxPipeline) != VK_SUCCESS)
	{
		std::cout << "Failed to create skybox pipeline\n";
		return 1;
	}

	float lastTime = 0.0f;
	float deltaTime = 0.0f;

	Camera camera;
	camera.SetPosition(glm::vec3(0.0f, 0.5f, 1.5f));
	camera.SetProjectionMatrix(85.0f, width, height, 0.1f, 10.0f);

	while (!glfwWindowShouldClose(window.GetHandle()))
	{
		window.UpdateInput();

		double currentTime = glfwGetTime();
		deltaTime = (float)currentTime - lastTime;
		lastTime = (float)currentTime;

		camera.Update(deltaTime, true, true);

		renderer.BeginFrame();

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto curTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();

		CameraUBO ubo = {};
		//ubo.model = glm::mat4(1.0f);
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		//ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(-180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		ubo.view = camera.GetViewMatrix();
		ubo.proj = camera.GetProjectionMatrix();
		ubo.proj[1][1] *= -1;

		void* data;
		vkMapMemory(device, cameraUBO.GetBufferMemory(), renderer.GetCurrentFrame() * (cameraUBO.GetSize() / MAX_FRAMES_IN_FLIGHT), sizeof(CameraUBO), 0, &data);
		memcpy(data, &ubo, sizeof(CameraUBO));
		vkUnmapMemory(device, cameraUBO.GetBufferMemory());


		renderer.BeginCmdRecording();
		renderer.BeginDefaultRenderPass();

		VkCommandBuffer cmdBuffer = renderer.GetCurrentCmdBuffer();

		VkBuffer vertexBuffers[] = { VK_NULL_HANDLE };
		VkDeviceSize offsets[] = { 0 };

		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &camUBOSet[renderer.GetCurrentFrame()], 0, nullptr);

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vertexBuffers[0] = model.GetVertexBuffer().GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuffer, model.GetIndexBuffer().GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &modelSet, 0, nullptr);
		vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(model.GetIndexCount()), 1, 0, 0, 0);

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
		vertexBuffers[0] = skyboxVB.GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &skyboxSet, 0, nullptr);
		vkCmdDraw(cmdBuffer, 36, 1, 0, 0);

		renderer.EndDefaultRenderPass();
		renderer.EndCmdRecording();

		renderer.EndFrame();
	}

	vkDeviceWaitIdle(device);

	vkDestroyDescriptorSetLayout(device, uboSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, texturesSetLayout, nullptr);

	cameraUBO.Dispose(device);

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

	renderer.Dispose();
	base.Dispose();

	glfwTerminate();

	return 0;
}