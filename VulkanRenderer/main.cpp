#include "Window.h"
#include "Input.h"
#include "Random.h"
#include "Camera.h"
#include "VKRenderer.h"
#include "VertexTypes.h"
#include "VKFramebuffer.h"
#include "ParticleManager.h"
#include "ModelManager.h"
#include "Skybox.h"
#include "UniformBufferTypes.h"
#include "EntityManager.h"
#include "TransformManager.h"
#include "Allocator.h"

#include "glm/gtc/matrix_transform.hpp"

#include <iostream>
#include <set>
#include <array>
#include <chrono>

unsigned int width = 640;
unsigned int height = 480;

int main()
{
	const int MAX_FRAMES_IN_FLIGHT = 2;
	
	Random::Init();
	InputManager inputManager;
	Window window;
	window.Init(&inputManager, width, height);

	Allocator allocator;
	EntityManager entityManager;
	TransformManager transformManager;
	transformManager.Init(&allocator, 10);

	Entity e[2] = { entityManager.Create(), entityManager.Create() };
	transformManager.AddTransform(e[0]);
	transformManager.AddTransform(e[1]);


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

	// Offscreen framebuffer
	TextureParams colorParams = {};
	colorParams.format = VK_FORMAT_R8G8B8A8_UNORM;
	colorParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	colorParams.filter = VK_FILTER_LINEAR;
	TextureParams depthParams = {};
	depthParams.format = vkutils::FindSupportedDepthFormat(base.GetPhysicalDevice());

	FramebufferParams fbParams = {};
	fbParams.createColorTexture = true;
	fbParams.createDepthTexture = true;
	fbParams.sampleDepth = false;
	fbParams.colorTexturesParams.push_back(colorParams);
	fbParams.depthTexturesParams = depthParams;

	VKFramebuffer offscreenFB;
	offscreenFB.Create(base, fbParams, width, height);

	// Shadow mapping framebuffer
	FramebufferParams shadowFBParams = {};
	shadowFBParams.createColorTexture = false;
	shadowFBParams.createDepthTexture = true;
	shadowFBParams.sampleDepth = true;
	shadowFBParams.depthTexturesParams.format = VK_FORMAT_D16_UNORM;
	shadowFBParams.depthTexturesParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	VkFilter filter = vkutils::IsFormatFilterable(base.GetPhysicalDevice(), VK_FORMAT_D16_UNORM, VK_IMAGE_TILING_OPTIMAL) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

	shadowFBParams.depthTexturesParams.filter = filter;

	VKFramebuffer shadowFB;
	shadowFB.Create(base, shadowFBParams, 1024, 1024);


	ModelManager modelManager;
	if (!modelManager.Init(renderer, offscreenFB.GetRenderPass()))
	{
		std::cout << "Failed to init model manager\n";
		return 1;
	}

	
	modelManager.AddModel(renderer, "Data/Models/trash_can.obj", "Data/Models/trash_can_d.jpg");
	modelManager.AddModel(renderer, "Data/Models/floor.obj", "Data/Models/floor.jpg");

	std::vector<std::string> faces(6);
	faces[0] = "Data/Textures/left.png";
	faces[1] = "Data/Textures/right.png";
	faces[2] = "Data/Textures/up.png";
	faces[3] = "Data/Textures/down.png";
	faces[4] = "Data/Textures/front.png";
	faces[5] = "Data/Textures/back.png";

	Skybox skybox;
	if (!skybox.Load(renderer, faces, offscreenFB.GetRenderPass()))
	{
		std::cout << "Failed to load skybox\n";
		return 1;
	}
	
	ParticleManager particleManager;
	if (!particleManager.Init(renderer, offscreenFB.GetRenderPass()))
		return 1;

	if (!particleManager.AddParticleSystem(renderer, "Data/Textures/particleTexture.png", 10))
	{
		std::cout << "Failed to add particle system\n";
		return 1;
	}


	TextureParams storageTexParams = {};
	storageTexParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	storageTexParams.filter = VK_FILTER_LINEAR;
	storageTexParams.format = VK_FORMAT_R8G8B8A8_UNORM;

	VKTexture2D storageTexture;
	storageTexture.CreateWithData(base, storageTexParams, 256, 256, nullptr);


	
	// We use a buffer with twice (or triple) the size and then offset into it so we don't have to create multiple buffers
	// Buffer create function takes care of aligment if the buffer is a ubo
	VKBuffer cameraUBO;
	cameraUBO.Create(&base, sizeof(CameraUBO) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Create a large buffer which will hold the model matrices
	VKBuffer instanceDataBuffer;
	instanceDataBuffer.Create(&base, sizeof(glm::mat4) * 32, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = cameraUBO.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	renderer.UpdateGlobalBuffersSet(bufferInfo, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

	bufferInfo.buffer = instanceDataBuffer.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	renderer.UpdateGlobalBuffersSet(bufferInfo, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imageInfo.imageView = shadowFB.GetDepthTexture().GetImageView();
	imageInfo.sampler = shadowFB.GetDepthTexture().GetSampler();

	VkDescriptorImageInfo imageInfo2 = {};
	imageInfo2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo2.imageView = storageTexture.GetImageView();
	imageInfo2.sampler = storageTexture.GetSampler();

	renderer.UpdateGlobalTexturesSet(imageInfo, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	renderer.UpdateGlobalTexturesSet(imageInfo2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);



	PipelineInfo pipeInfo = VKPipeline::DefaultFillStructs();

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = surfaceExtent;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
	};


	// No need to set the viewport because it's dynamic
	pipeInfo.viewportState.viewportCount = 1;
	pipeInfo.viewportState.scissorCount = 1;
	pipeInfo.viewportState.pScissors = &scissor;

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	pipeInfo.dynamicState.dynamicStateCount = 1;
	pipeInfo.dynamicState.pDynamicStates = dynamicStates;

	// OFFSCREEN

	// Post quad is rendered with no buffers
	pipeInfo.vertexInput.vertexBindingDescriptionCount = 0;
	pipeInfo.vertexInput.pVertexBindingDescriptions = nullptr;
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = 0;
	pipeInfo.vertexInput.pVertexAttributeDescriptions = nullptr;

	pipeInfo.rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
	pipeInfo.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// Change it back to LESS from the skybox pipeline
	pipeInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;

	VKShader postQuadShader;
	postQuadShader.LoadShader(device, "Data/Shaders/post_quad_vert.spv", "Data/Shaders/post_quad_frag.spv");

	VKPipeline postQuadPipeline;
	if(!postQuadPipeline.Create(device, pipeInfo, renderer.GetPipelineLayout(), postQuadShader, renderer.GetDefaultRenderPass()))
		return 1;

	// SHADOW MAPPING

	VkVertexInputBindingDescription bindingDesc = {};
	bindingDesc = {};
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

	pipeInfo.vertexInput.vertexBindingDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexBindingDescriptions = &bindingDesc;
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = 3;
	pipeInfo.vertexInput.pVertexAttributeDescriptions = attribDesc;

	
	pipeInfo.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pipeInfo.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;

	VKShader shadowShader;
	shadowShader.LoadShader(device, "Data/Shaders/shadow_vert.spv", "Data/Shaders/shadow_frag.spv");

	VKPipeline shadowPipeline;
	if(!shadowPipeline.Create(device, pipeInfo, renderer.GetPipelineLayout(), shadowShader, shadowFB.GetRenderPass()))
		return 1;


	VkDescriptorSet quadSet = renderer.AllocateUserTextureDescriptorSet();
	
	imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo2.imageView = offscreenFB.GetFirstColorTexture().GetImageView();
	imageInfo2.sampler = offscreenFB.GetFirstColorTexture().GetSampler();

	VkWriteDescriptorSet descriptorWrites = {};
	descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites.dstBinding = 0;
	descriptorWrites.dstArrayElement = 0;
	descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites.descriptorCount = 1;
	descriptorWrites.dstSet = quadSet;
	descriptorWrites.pImageInfo = &imageInfo2;

	vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);

	// Compute

	VkDescriptorSetLayoutBinding computeSetLayoutBinding = {};
	computeSetLayoutBinding.binding = 0;
	computeSetLayoutBinding.descriptorCount = 1;
	computeSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	computeSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo computeSetLayoutInfo = {};
	computeSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	computeSetLayoutInfo.bindingCount = 1;
	computeSetLayoutInfo.pBindings = &computeSetLayoutBinding;

	VkDescriptorSetLayout computeSetLayout;

	if (vkCreateDescriptorSetLayout(device, &computeSetLayoutInfo, nullptr, &computeSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor set layout\n";
		return 1;
	}	

	VkPipelineLayoutCreateInfo computePipeLayoutInfo = {};
	computePipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computePipeLayoutInfo.setLayoutCount = 1;
	computePipeLayoutInfo.pSetLayouts = &computeSetLayout;

	VkPipelineLayout computePipelineLayout;

	if (vkCreatePipelineLayout(device, &computePipeLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create pipeline layout\n";
		return 1;
	}

	VkDescriptorSet computeSet = renderer.AllocateSetFromLayout(computeSetLayout);

	VkDescriptorImageInfo compImgInfo = {};
	compImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	compImgInfo.imageView = storageTexture.GetImageView();
	//compImgInfo.sampler = ;

	VkWriteDescriptorSet computeWrite = {};
	computeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	computeWrite.descriptorCount = 1;
	computeWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	computeWrite.dstArrayElement = 0;
	computeWrite.dstBinding = 0;
	computeWrite.dstSet = computeSet;
	computeWrite.pImageInfo = &compImgInfo;

	vkUpdateDescriptorSets(device, 1, &computeWrite, 0, nullptr);


	VKShader computeShader;
	if (!computeShader.LoadShader(device, "Data/Shaders/compute.spv"))
	{
		std::cout << "Failed to create compute shader\n";
		return 1;
	}

	VkPipelineShaderStageCreateInfo computeStageInfo = computeShader.GetComputeStageInfo();

	VkComputePipelineCreateInfo computePipeInfo = {};
	computePipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipeInfo.layout = computePipelineLayout;
	computePipeInfo.stage = computeStageInfo;

	VkPipeline computePipeline;

	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipeInfo, nullptr, &computePipeline) != VK_SUCCESS)
	{
		std::cout << "Failed to create compute pipeline\n";
		return 1;
	}
	
	// Semaphore for compute and graphics sync
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkSemaphore computeSemaphore;
	VkSemaphore graphicsSemaphore;

	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeSemaphore) != VK_SUCCESS)
	{
		std::cout << "Failed to create compute semaphore\n";
		return 1;
	}
	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &graphicsSemaphore) != VK_SUCCESS)
	{
		std::cout << "Failed to create graphics semaphore\n";
		return 1;
	}


	// Signal the semaphore
	VkSubmitInfo semaInfo = {};
	semaInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	semaInfo.signalSemaphoreCount = 1;
	semaInfo.pSignalSemaphores = &graphicsSemaphore;

	if (vkQueueSubmit(base.GetGraphicsQueue(), 1, &semaInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		std::cout << "Failed to submit\n";
		return 1;
	}
	vkQueueWaitIdle(base.GetGraphicsQueue());

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence computeFence;

	if (vkCreateFence(device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS)
	{
		std::cout << "Failed to create fence\n";
		return 1;
	}


	const vkutils::QueueFamilyIndices& indices = base.GetQueueFamilyIndices();

	// Record the compute command buffer
	VkCommandBuffer computeCmdBuffer = renderer.CreateComputeCommandBuffer(true);


	VkImageSubresourceRange range = {};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseArrayLayer = 0;
	range.baseMipLevel = 0;
	range.layerCount = 1;
	range.levelCount = 1;

	if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
	{
		// Acquire
		VkImageMemoryBarrier acquireBarrier = {};
		acquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		acquireBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		acquireBarrier.srcAccessMask = 0;
		acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		acquireBarrier.srcQueueFamilyIndex = indices.graphicsFamilyIndex;
		acquireBarrier.dstQueueFamilyIndex = indices.computeFamilyIndex;
		acquireBarrier.subresourceRange = range;
		acquireBarrier.image = storageTexture.GetImage();

		vkCmdPipelineBarrier(computeCmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &acquireBarrier);
	}

	vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeSet, 0, nullptr);
	vkCmdDispatch(computeCmdBuffer, 256 / 16, 256 / 16, 1);

	if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
	{
		// Release
		VkImageMemoryBarrier releaseBarrier = {};
		releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		releaseBarrier.dstAccessMask = 0;
		releaseBarrier.srcQueueFamilyIndex = indices.computeFamilyIndex;
		releaseBarrier.dstQueueFamilyIndex = indices.graphicsFamilyIndex;
		releaseBarrier.subresourceRange = range;
		releaseBarrier.image = storageTexture.GetImage();

		vkCmdPipelineBarrier(computeCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &releaseBarrier);
	}

	vkEndCommandBuffer(computeCmdBuffer);

	// Transfer ownership
	// If graphics and compute queue family indices differ, acquire and immediately release the storage image,
	// so that the initial acquire from the graphics command buffers are matched up properly

	if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
	{

		// Release ownership from graphics queue to compute queue
		VkImageSubresourceRange range = {};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseArrayLayer = 0;
		range.baseMipLevel = 0;
		range.layerCount = 1;
		range.levelCount = 1;

		VkCommandBuffer cmd = renderer.CreateGraphicsCommandBuffer(true);

		VkImageMemoryBarrier releaseBarrier = {};
		releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.srcAccessMask = 0;
		releaseBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		releaseBarrier.srcQueueFamilyIndex = indices.graphicsFamilyIndex;
		releaseBarrier.dstQueueFamilyIndex = indices.computeFamilyIndex;
		releaseBarrier.subresourceRange = range;
		releaseBarrier.image = storageTexture.GetImage();

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &releaseBarrier);

		vkEndCommandBuffer(cmd);

		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &cmd;

		if (vkQueueSubmit(base.GetGraphicsQueue(), 1, &info, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			std::cout << "Failed to submit\n";
			return 1;
		}
		vkQueueWaitIdle(base.GetGraphicsQueue());		// Or wait for fence?

		renderer.FreeGraphicsCommandBuffer(cmd);
	}

	

	// Create quad to display the image create by the compute shader

	glm::vec4 quadVertices[] = {
		glm::vec4(-1.0f, -1.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f,	0.0f, 1.0f),
		glm::vec4(1.0f, -1.0f,	1.0f, 0.0f),
		
		glm::vec4(1.0f, -1.0f,	1.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f,	0.0f, 1.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
	};

	VKBuffer quadVertexStagingBuffer;
	quadVertexStagingBuffer.Create(&base, sizeof(quadVertices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	unsigned int vertexSize = quadVertexStagingBuffer.GetSize();

	void* mapped = quadVertexStagingBuffer.Map(device, 0, vertexSize);
	memcpy(mapped, quadVertices, (size_t)vertexSize);
	quadVertexStagingBuffer.Unmap(device);

	VKBuffer quadVb;
	quadVb.Create(&base, sizeof(quadVertices), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	base.CopyBuffer(quadVertexStagingBuffer, quadVb, vertexSize);
	quadVertexStagingBuffer.Dispose(device);

	VkDescriptorSet quadStorageSet;

	VkVertexInputBindingDescription quadBindingDesc = {};
	quadBindingDesc.binding = 0;
	quadBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	quadBindingDesc.stride = sizeof(glm::vec4);

	VkVertexInputAttributeDescription quadAttribDesc = {};
	quadAttribDesc.binding = 0;
	quadAttribDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	quadAttribDesc.location = 0;
	quadAttribDesc.offset = 0;

	pipeInfo.depthStencilState.depthWriteEnable = VK_TRUE;

	colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	pipeInfo.vertexInput.vertexBindingDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexBindingDescriptions = &quadBindingDesc;
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexAttributeDescriptions = &quadAttribDesc;

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	VKShader quadStorageShader;
	quadStorageShader.LoadShader(device, "Data/Shaders/quad_Vert.spv", "Data/Shaders/quad_frag.spv");

	VKPipeline quadStoragePipeline;
	if (!quadStoragePipeline.Create(device, pipeInfo, renderer.GetPipelineLayout(), quadStorageShader, offscreenFB.GetRenderPass()))
		return 1;



	// Create mipmaps

	VkCommandBuffer cmdBuffer = renderer.CreateGraphicsCommandBuffer(true);

	const std::vector<RenderModel>& models = modelManager.GetRenderModels();

	for (size_t i = 0; i < models.size(); i++)
	{
		renderer.CreateMipMaps(cmdBuffer, models[i].texture);
	}

	const std::vector<ParticleSystem>& particleSystems = particleManager.GetParticlesystems();

	for (size_t i = 0; i < particleSystems.size(); i++)
	{
		renderer.CreateMipMaps(cmdBuffer, particleSystems[i].GetTexture());
	}

	if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS)
	{
		std::cout << "Failed to end command buffer\n";
		return 1;
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	VkFence fence;

	fenceInfo.flags = 0;

	if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
	{
		std::cout << "Failed to create fence\n";
		return 1;
	}
	if (vkQueueSubmit(base.GetGraphicsQueue(), 1, &submitInfo, fence) != VK_SUCCESS)
	{
		std::cout << "Failed to submit\n";
		return 1;
	}
	if (vkWaitForFences(device, 1, &fence, VK_TRUE, 100000000000) != VK_SUCCESS)
	{
		std::cout << "wait failed\n";
		return 1;
	}

	vkDestroyFence(device, fence, nullptr);

	renderer.FreeGraphicsCommandBuffer(cmdBuffer);

	float lastTime = 0.0f;
	float deltaTime = 0.0f;

	Camera camera;
	camera.SetPosition(glm::vec3(0.0f, 0.5f, 1.5f));
	camera.SetProjectionMatrix(85.0f, width, height, 0.1f, 20.0f);

	glm::mat4 *camerasData = (glm::mat4*)malloc(static_cast<size_t>(cameraUBO.GetSize()));

	unsigned int minUBOAlignment = static_cast<unsigned int>(base.GetPhysicalDeviceLimits().minUniformBufferOffsetAlignment);
	unsigned int alignSingleUBOSize = sizeof(CameraUBO);

	if (minUBOAlignment > 0)
		alignSingleUBOSize = (alignSingleUBOSize + minUBOAlignment - 1) & ~(minUBOAlignment - 1);

	unsigned int currentCamera = 0;
	unsigned int maxCamera = 2;

	while (!glfwWindowShouldClose(window.GetHandle()))
	{
		window.UpdateInput();

		double currentTime = glfwGetTime();
		deltaTime = (float)currentTime - lastTime;
		lastTime = (float)currentTime;

		camera.Update(deltaTime, true, true);

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto curTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();

		// Set camera at 0 at the start of the frame
		currentCamera = 0;

		glm::mat4 lightProj = glm::ortho(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 10.0f);
		glm::mat4 lightView = glm::lookAt(glm::vec3(0.0f, 2.0f, 2.5f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 00.0f));
		glm::mat4 lightSpaceMatrix = lightProj * lightView;

		//glm::mat4 modelMatrix = glm::rotate(glm::mat4(1.0f), time * glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		CameraUBO* ubo = (CameraUBO*)(((uint64_t)camerasData + (currentCamera * alignSingleUBOSize)));
		if (ubo)
		{
			
			ubo->view = lightView;
			ubo->proj = lightProj;
			ubo->projView = lightProj * lightView;
			ubo->proj[1][1] *= -1;
		}


		renderer.WaitForFrameFences();
		renderer.BeginCmdRecording();

		VkCommandBuffer cmdBuffer = renderer.GetCurrentCmdBuffer();

		VkPipelineLayout pipelineLayout = renderer.GetPipelineLayout();
		VkDescriptorSet globalBuffersSet = renderer.GetGlobalBuffersSet();
		VkDescriptorSet globalTexturesSet = renderer.GetGlobalTexturesSet();

		// Bind the camera descriptor set with the ubo
		//vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &buffersSet, 0, nullptr);
		uint32_t dynamicOffset = static_cast<uint32_t>(currentCamera) * alignSingleUBOSize;
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &globalBuffersSet, 1, &dynamicOffset);

		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &globalTexturesSet, 0, nullptr);

		currentCamera++;

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;		
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// SHADOW MAPPING

		viewport.width = shadowFB.GetWidth();
		viewport.height = shadowFB.GetHeight();

		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

		VkClearValue depthClearValue = {};
		depthClearValue.depthStencil = { 1.0f, 0 };

		renderer.BeginRenderPass(cmdBuffer, shadowFB, 1, &depthClearValue);
		modelManager.Render(cmdBuffer, pipelineLayout, shadowPipeline.GetPipeline());
		vkCmdEndRenderPass(cmdBuffer);

		// OFFSCREEN

		// Set the normal camera
		ubo = (CameraUBO*)(((uint64_t)camerasData + (currentCamera * alignSingleUBOSize)));
		if (ubo)
		{
			ubo->view = camera.GetViewMatrix();
			ubo->proj = camera.GetProjectionMatrix();
			ubo->projView = ubo->proj * ubo->view;
			ubo->proj[1][1] *= -1;
			ubo->lightSpaceMatrix = lightSpaceMatrix;
		}

		dynamicOffset = static_cast<uint32_t>(currentCamera) * alignSingleUBOSize;
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &globalBuffersSet, 1, &dynamicOffset);

		viewport.width = (float)surfaceExtent.width;
		viewport.height = (float)surfaceExtent.height;

		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);


		if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
		{
			// Acquire
			VkImageMemoryBarrier acquireBarrier = {};
			acquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			acquireBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			acquireBarrier.srcAccessMask = 0;
			acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			acquireBarrier.srcQueueFamilyIndex = indices.computeFamilyIndex;
			acquireBarrier.dstQueueFamilyIndex = indices.graphicsFamilyIndex;
			acquireBarrier.subresourceRange = range;
			acquireBarrier.image = storageTexture.GetImage();

			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &acquireBarrier);

			//renderer.AcquireImageBarrier(cmdBuffer, storageTexture, indices.computeFamilyIndex, indices.graphicsFamilyIndex);
		}
		


		VkClearValue clearValues[2] = {};
		clearValues[0].color = { 0.3f, 0.3f, 0.3f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderer.BeginRenderPass(cmdBuffer, offscreenFB, 2, clearValues);
	
		modelManager.Render(cmdBuffer, pipelineLayout, VK_NULL_HANDLE);

		VkBuffer vertexBuffers[] = { VK_NULL_HANDLE };
		VkDeviceSize offsets[] = { 0 };

		vertexBuffers[0] = quadVb.GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadStoragePipeline.GetPipeline());
		vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

		
		// Render skybox as last
		skybox.Render(cmdBuffer, pipelineLayout);

		// Particle systems have to be rendered after the skybox
		particleManager.Render(cmdBuffer, renderer.GetPipelineLayout());


		vkCmdEndRenderPass(cmdBuffer);

		if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
		{
			// Release
			VkImageMemoryBarrier releaseBarrier = {};
			releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			releaseBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			releaseBarrier.dstAccessMask = 0;
			releaseBarrier.srcQueueFamilyIndex = indices.graphicsFamilyIndex;
			releaseBarrier.dstQueueFamilyIndex = indices.computeFamilyIndex;
			releaseBarrier.subresourceRange = range;
			releaseBarrier.image = storageTexture.GetImage();

			vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &releaseBarrier);

			//renderer.ReleaseImageBarrier(cmdBuffer, storageTexture, indices.graphicsFamilyIndex, indices.computeFamilyIndex);
		}


		// Normal pass
		renderer.BeginDefaultRenderPass();

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postQuadPipeline.GetPipeline());
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &quadSet, 0, nullptr);
		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		renderer.EndDefaultRenderPass();
		renderer.EndCmdRecording();


		// Update buffers

		void* mapped = cameraUBO.Map(device, 0, VK_WHOLE_SIZE);
		memcpy(mapped, camerasData, static_cast<size_t>(currentCamera + 1) * alignSingleUBOSize);
		cameraUBO.Unmap(device);

		//transformManager.setlo
		transformManager.SetLocalPosition(e[0], glm::vec3(0.0f, 0.0f, 8.0f));

		// Update model matrices
		mapped = instanceDataBuffer.Map(device, 0, VK_WHOLE_SIZE);
		size_t offset = 0;
		for (size_t i = 0; i < modelManager.GetRenderModels().size(); i++)
		{
			void* ptr = (char*)mapped + offset;
			memcpy(ptr, &transformManager.GetLocalToWorld(e[i]), sizeof(glm::mat4));
			offset += sizeof(glm::mat4);
		}

		instanceDataBuffer.Unmap(device);

		particleManager.Update(device, deltaTime);


		// Compute
		
		VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

		VkSubmitInfo computeSubmitInfo = {};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.waitSemaphoreCount = 1;
		computeSubmitInfo.pWaitSemaphores = &graphicsSemaphore;
		computeSubmitInfo.pWaitDstStageMask = &waitStage;
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &computeCmdBuffer;
		computeSubmitInfo.signalSemaphoreCount = 1;
		computeSubmitInfo.pSignalSemaphores = &computeSemaphore;

		vkWaitForFences(device, 1, &computeFence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &computeFence);

		if (vkQueueSubmit(base.GetComputeQueue(), 1, &computeSubmitInfo, computeFence) != VK_SUCCESS)
		{
			std::cout << "Failed to submit\n";
			return 1;
		}
		
		renderer.AcquireNextImage();
		renderer.Present(graphicsSemaphore, computeSemaphore);
	}

	vkDeviceWaitIdle(device);

	if (camerasData)
		free(camerasData);

	quadStoragePipeline.Dispose(device);
	quadStorageShader.Dispose(device);
	quadVb.Dispose(device);

	cameraUBO.Dispose(device);
	offscreenFB.Dispose(device);
	shadowFB.Dispose(device);
	instanceDataBuffer.Dispose(device);


	vkDestroyFence(device, computeFence, nullptr);
	storageTexture.Dispose(device);
	vkDestroyPipeline(device, computePipeline, nullptr);
	vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, computeSetLayout, nullptr);
	computeShader.Dispose(device);
	vkDestroySemaphore(device, computeSemaphore, nullptr);
	vkDestroySemaphore(device, graphicsSemaphore, nullptr);
	
	modelManager.Dispose(device);
	particleManager.Dispose(device);
	skybox.Dispose(device);
	
	postQuadShader.Dispose(device);
	shadowShader.Dispose(device);
	postQuadPipeline.Dispose(device);
	shadowPipeline.Dispose(device);

	transformManager.Dispose();
	renderer.Dispose();
	base.Dispose();

	glfwTerminate();

	return 0;
}