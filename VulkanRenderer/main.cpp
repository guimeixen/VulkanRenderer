#include "Window.h"
#include "Input.h"
#include "Random.h"
#include "Camera.h"
#include "VKRenderer.h"
#include "Model.h"
#include "VertexTypes.h"
#include "VKFramebuffer.h"
#include "VKPipeline.h"
#include "ParticleSystem.h"

#include "glm/gtc/matrix_transform.hpp"

#include <iostream>
#include <string>
#include <set>
#include <array>
#include <chrono>

unsigned int width = 640;
unsigned int height = 480;

void CreateMipMaps(VkCommandBuffer cmdBuffer, const VKTexture2D& texture)
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

int main()
{
	const int MAX_FRAMES_IN_FLIGHT = 2;
	
	VkPipelineLayout pipelineLayout;
	VkDescriptorSet globalBuffersSet;
	VkDescriptorSet globalTexturesSet;

	VkDescriptorSet modelSet;
	VkDescriptorSet floorSet;
	VkDescriptorSet skyboxSet;
	VkDescriptorSet psSet;

	VKBuffer skyboxVB;
	
	Model model;

	Random::Init();
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



	TextureParams textureParams = {};
	textureParams.format = VK_FORMAT_R8G8B8A8_SRGB;
	textureParams.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	textureParams.filter = VK_FILTER_LINEAR;

	VKTexture2D modelTexture;
	modelTexture.LoadFromFile(base, "Data/Models/trash_can_d.jpg", textureParams);

	model.Load("Data/Models/trash_can.obj", base);

	Model floorModel;
	floorModel.Load("Data/Models/floor.obj", base);

	VKTexture2D floorTexture;
	floorTexture.LoadFromFile(base, "Data/Models/floor.jpg", textureParams);

	// Load cubemap
	std::vector<std::string> faces(6);
	faces[0] = "Data/Textures/left.png";
	faces[1] = "Data/Textures/right.png";
	faces[2] = "Data/Textures/up.png";
	faces[3] = "Data/Textures/down.png";
	faces[4] = "Data/Textures/front.png";
	faces[5] = "Data/Textures/back.png";

	TextureParams cubemapParams = {};
	cubemapParams.format = VK_FORMAT_R8G8B8A8_SRGB;
	cubemapParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cubemapParams.filter = VK_FILTER_LINEAR;

	VKTexture2D cubemap;
	cubemap.LoadCubemapFromFiles(base, faces, cubemapParams);

	ParticleSystem particleSystem;
	if (!particleSystem.Init(base, "Data/Textures/particleTexture.png", 10))
		return false;

	TextureParams storageTexParams = {};
	storageTexParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	storageTexParams.filter = VK_FILTER_LINEAR;
	storageTexParams.format = VK_FORMAT_R8G8B8A8_UNORM;

	VKTexture2D storageTexture;
	storageTexture.CreateWithData(base, storageTexParams, 256, 256, nullptr);


	struct CameraUBO
	{
		glm::mat4 proj;
		glm::mat4 view;
		glm::mat4 model;
		glm::mat4 lightSpaceMatrix;
	};

	// Global Buffers

	VkDescriptorSetLayoutBinding cameraLayoutBinding = {};
	cameraLayoutBinding.binding = 0;
	cameraLayoutBinding.descriptorCount = 1;
	cameraLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	cameraLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo buffersSetLayoutInfo = {};
	buffersSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	buffersSetLayoutInfo.bindingCount = 1;
	buffersSetLayoutInfo.pBindings = &cameraLayoutBinding;

	VkDescriptorSetLayout buffersSetLayout;

	if (vkCreateDescriptorSetLayout(device, &buffersSetLayoutInfo, nullptr, &buffersSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create buffers descriptor set layout\n";
		return 1;
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

	VkDescriptorSetLayoutCreateInfo texturesSetLayoutInfo = {};
	texturesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	texturesSetLayoutInfo.bindingCount = 2;
	texturesSetLayoutInfo.pBindings = texturesSetLayoutBindings;

	VkDescriptorSetLayout texturesSetLayout;

	if (vkCreateDescriptorSetLayout(device, &texturesSetLayoutInfo, nullptr, &texturesSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create textures descriptor set layout\n";
		return 1;
	}

	// User Textures

	VkDescriptorSetLayoutBinding userTextureLayoutBinding = {};
	userTextureLayoutBinding.binding = 0;
	userTextureLayoutBinding.descriptorCount = 1;
	userTextureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	userTextureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding userTexturesSetLayoutBindings[] = { userTextureLayoutBinding };

	VkDescriptorSetLayoutCreateInfo userTexturesSetLayoutInfo = {};
	userTexturesSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	userTexturesSetLayoutInfo.bindingCount = 1;
	userTexturesSetLayoutInfo.pBindings = userTexturesSetLayoutBindings;

	VkDescriptorSetLayout userTexturesSetLayout;

	if (vkCreateDescriptorSetLayout(device, &userTexturesSetLayoutInfo, nullptr, &userTexturesSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create textures descriptor set layout\n";
		return 1;
	}

	// We use a buffer with twice (or triple) the size and then offset into it so we don't have to create multiple buffers
	// Buffer create function takes care of aligment if the buffer is a ubo
	VKBuffer cameraUBO;
	cameraUBO.Create(&base, sizeof(CameraUBO) * 2, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkDescriptorPoolSize poolSizes[3] = {};
	poolSizes[0].descriptorCount = 1;
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

	poolSizes[1].descriptorCount = 10;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

	poolSizes[2].descriptorCount = 1;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

	VkDescriptorPoolCreateInfo descPoolInfo = {};
	descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolInfo.maxSets = 8;
	descPoolInfo.poolSizeCount = 3;
	descPoolInfo.pPoolSizes = poolSizes;

	VkDescriptorPool descriptorPool;

	if (vkCreateDescriptorPool(device, &descPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor pool\n";
		return 1;
	}
	std::cout << "Created descriptor pool\n";

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &buffersSetLayout;

	if (vkAllocateDescriptorSets(device, &setAllocInfo, &globalBuffersSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &texturesSetLayout;
	if (vkAllocateDescriptorSets(device, &setAllocInfo, &globalTexturesSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &userTexturesSetLayout;
	if (vkAllocateDescriptorSets(device, &setAllocInfo, &modelSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	if (vkAllocateDescriptorSets(device, &setAllocInfo, &floorSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	if (vkAllocateDescriptorSets(device, &setAllocInfo, &skyboxSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	if (vkAllocateDescriptorSets(device, &setAllocInfo, &psSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

	std::cout << "Allocated descriptor sets\n";

	// Global buffers set update

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = cameraUBO.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	VkWriteDescriptorSet descWrite = {};
	descWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descWrite.dstSet = globalBuffersSet;
	descWrite.dstBinding = 0;
	descWrite.dstArrayElement = 0;
	descWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	descWrite.descriptorCount = 1;
	descWrite.pBufferInfo = &bufferInfo;

	vkUpdateDescriptorSets(device, 1, &descWrite, 0, nullptr);

	// Global Textures set update

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imageInfo.imageView = shadowFB.GetDepthTexture().GetImageView();
	imageInfo.sampler = shadowFB.GetDepthTexture().GetSampler();

	VkDescriptorImageInfo imageInfo2 = {};
	imageInfo2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo2.imageView = storageTexture.GetImageView();
	imageInfo2.sampler = storageTexture.GetSampler();

	VkWriteDescriptorSet shadowMapWrite = {};
	shadowMapWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowMapWrite.dstSet = globalTexturesSet;
	shadowMapWrite.dstBinding = 0;
	shadowMapWrite.dstArrayElement = 0;
	shadowMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapWrite.descriptorCount = 1;
	shadowMapWrite.pImageInfo = &imageInfo;

	VkWriteDescriptorSet storageWrite = {};
	storageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	storageWrite.dstSet = globalTexturesSet;
	storageWrite.dstBinding = 1;
	storageWrite.dstArrayElement = 0;
	storageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	storageWrite.descriptorCount = 1;
	storageWrite.pImageInfo = &imageInfo2;

	VkWriteDescriptorSet writes[] = { shadowMapWrite, storageWrite };

	vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);


	// Model

	imageInfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo2.imageView = modelTexture.GetImageView();
	imageInfo2.sampler = modelTexture.GetSampler();

	VkWriteDescriptorSet descriptorWrites = {};
	descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites.dstSet = modelSet;
	descriptorWrites.dstBinding = 0;
	descriptorWrites.dstArrayElement = 0;
	descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites.descriptorCount = 1;
	descriptorWrites.pImageInfo = &imageInfo2;

	vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);

	// Floor
	imageInfo2.imageView = floorTexture.GetImageView();
	imageInfo2.sampler = floorTexture.GetSampler();

	descriptorWrites.dstSet = floorSet;
	descriptorWrites.pImageInfo = &imageInfo2;

	vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);


	// Skybox
	imageInfo2.imageView = cubemap.GetImageView();
	imageInfo2.sampler = cubemap.GetSampler();

	descriptorWrites.dstSet = skyboxSet;
	descriptorWrites.pImageInfo = &imageInfo2;

	vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);

	// Particle system
	imageInfo2.imageView = particleSystem.GetTexture().GetImageView();
	imageInfo2.sampler = particleSystem.GetTexture().GetSampler();

	descriptorWrites.dstSet = psSet;
	descriptorWrites.pImageInfo = &imageInfo2;

	vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);


	// Create pipeline layout
	VkDescriptorSetLayout setLayouts[] = { buffersSetLayout, texturesSetLayout, userTexturesSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 3;
	pipelineLayoutInfo.pSetLayouts = setLayouts;
	pipelineLayoutInfo.pushConstantRangeCount = 0;
	pipelineLayoutInfo.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create pipeline layout\n";
		return 1;
	}
	std::cout << "Create pipeline layout\n";



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

	pipeInfo.vertexInput.vertexBindingDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexBindingDescriptions = &bindingDesc;
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = 3;
	pipeInfo.vertexInput.pVertexAttributeDescriptions = attribDesc;

	// No need to set the viewport because it's dynamic
	pipeInfo.viewportState.viewportCount = 1;
	pipeInfo.viewportState.scissorCount = 1;
	pipeInfo.viewportState.pScissors = &scissor;

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	pipeInfo.dynamicState.dynamicStateCount = 1;
	pipeInfo.dynamicState.pDynamicStates = dynamicStates;

	

	VKShader modelShader;
	modelShader.LoadShader(device, "Data/Shaders/shader_vert.spv", "Data/Shaders/shader_frag.spv");

	VKPipeline modelPipeline;
	if (!modelPipeline.Create(device, pipeInfo, pipelineLayout, modelShader, offscreenFB.GetRenderPass()))
		return 1;



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

	void* mapped = vertexStagingBuffer.Map(device, 0, vertexSize);
	memcpy(mapped, skyboxVertices, (size_t)vertexSize);
	vertexStagingBuffer.Unmap(device);

	skyboxVB.Create(&base, sizeof(skyboxVertices), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	base.CopyBuffer(vertexStagingBuffer, skyboxVB, vertexSize);

	vertexStagingBuffer.Dispose(device);


	bindingDesc.binding = 0;
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc.stride = sizeof(glm::vec3);

	attribDesc[0].binding = 0;
	attribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDesc[0].location = 0;
	attribDesc[0].offset = 0;

	pipeInfo.vertexInput.vertexBindingDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexBindingDescriptions = &bindingDesc;
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexAttributeDescriptions = attribDesc;

	// Make sure to use LEQUAL in depth testing, because we set the skybox depth to 1 in the vertex shader
	// and otherwise wouldn't render if we kept using just LESS
	pipeInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	VKShader skyboxShader;
	skyboxShader.LoadShader(device, "Data/Shaders/skybox_vert.spv", "Data/Shaders/skybox_frag.spv");

	VKPipeline skyboxPipeline;
	if (!skyboxPipeline.Create(device, pipeInfo, pipelineLayout, skyboxShader, offscreenFB.GetRenderPass()))
		return 1;

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
	if(!postQuadPipeline.Create(device, pipeInfo, pipelineLayout, postQuadShader, renderer.GetDefaultRenderPass()))
		return 1;

	// SHADOW MAPPING

	bindingDesc = {};
	bindingDesc.binding = 0;
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc.stride = sizeof(Vertex);

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
	if(!shadowPipeline.Create(device, pipeInfo, pipelineLayout, shadowShader, shadowFB.GetRenderPass()))
		return 1;



	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &userTexturesSetLayout;

	VkDescriptorSet quadSet;

	if (vkAllocateDescriptorSets(device, &setAllocInfo, &quadSet) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}


	imageInfo2.imageView = offscreenFB.GetFirstColorTexture().GetImageView();
	imageInfo2.sampler = offscreenFB.GetFirstColorTexture().GetSampler();

	descriptorWrites.dstSet = quadSet;
	descriptorWrites.pImageInfo = &imageInfo2;

	vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);

	// Particle system pipeline

	// Pipeline

	VkVertexInputBindingDescription psBindingDesc[2] = {};
	psBindingDesc[0].binding = 0;
	psBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	psBindingDesc[0].stride = sizeof(glm::vec4);
	psBindingDesc[1].binding = 1;
	psBindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
	psBindingDesc[1].stride = sizeof(ParticleInstanceData);

	VkVertexInputAttributeDescription psAttribDesc[3] = {};
	psAttribDesc[0].binding = 0;
	psAttribDesc[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	psAttribDesc[0].location = 0;
	psAttribDesc[0].offset = 0;

	psAttribDesc[1].binding = 1;
	psAttribDesc[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	psAttribDesc[1].location = 1;
	psAttribDesc[1].offset = 0;

	psAttribDesc[2].binding = 1;
	psAttribDesc[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	psAttribDesc[2].location = 2;
	psAttribDesc[2].offset = offsetof(ParticleInstanceData, color);

	// Don't write depth
	pipeInfo.depthStencilState.depthWriteEnable = VK_FALSE;

	colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	pipeInfo.vertexInput.vertexBindingDescriptionCount = 2;
	pipeInfo.vertexInput.pVertexBindingDescriptions = psBindingDesc;
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = 3;
	pipeInfo.vertexInput.pVertexAttributeDescriptions = psAttribDesc;

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	VKShader psShader;
	psShader.LoadShader(device, "Data/Shaders/particle_vert.spv", "Data/Shaders/particle_frag.spv");

	VKPipeline psPipeline;
	if (!psPipeline.Create(device, pipeInfo, pipelineLayout, psShader, offscreenFB.GetRenderPass()))
		return 1;

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

	VkDescriptorSet computeSet;

	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &computeSetLayout;

	if (vkAllocateDescriptorSets(device, &setAllocInfo, &computeSet) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate descriptor set\n";
		return 1;
	}

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

	if (vkQueueSubmit(base.GetComputeQueue(), 1, &semaInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		std::cout << "Failed to submit\n";
		return 1;
	}
	vkQueueWaitIdle(base.GetComputeQueue());

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence computeFence;

	if (vkCreateFence(device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS)
	{
		std::cout << "Failed to create fence\n";
		return 1;
	}

	// Record the compute command buffer
	VkCommandBuffer computeCmdBuffer = renderer.CreateComputeCommandBuffer(true);

	vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeSet, 0, nullptr);
	vkCmdDispatch(computeCmdBuffer, 256 / 16, 256 / 16, 1);

	vkEndCommandBuffer(computeCmdBuffer);


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
	vertexSize = quadVertexStagingBuffer.GetSize();

	mapped = quadVertexStagingBuffer.Map(device, 0, vertexSize);
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
	if (!quadStoragePipeline.Create(device, pipeInfo, pipelineLayout, quadStorageShader, offscreenFB.GetRenderPass()))
		return 1;



	// Create mipmaps

	VkCommandBuffer cmdBuffer = renderer.CreateGraphicsCommandBuffer(true);

	CreateMipMaps(cmdBuffer, modelTexture);
	CreateMipMaps(cmdBuffer, floorTexture);
	CreateMipMaps(cmdBuffer, particleSystem.GetTexture());

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

		glm::mat4 modelMatrix = glm::rotate(glm::mat4(1.0f), time * glm::radians(20.0f), glm::vec3(0.0f, 1.0f, 0.0f));

		CameraUBO* ubo = (CameraUBO*)(((uint64_t)camerasData + (currentCamera * alignSingleUBOSize)));
		if (ubo)
		{
			ubo->model = modelMatrix;
			ubo->view = lightView;
			ubo->proj = lightProj;
			ubo->proj[1][1] *= -1;
		}


		renderer.WaitForFrameFences();



		renderer.BeginCmdRecording();

		VkCommandBuffer cmdBuffer = renderer.GetCurrentCmdBuffer();

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

		viewport.width = 1024;
		viewport.height = 1024;

		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

		VkClearValue depthClearValue = {};
		depthClearValue.depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderBeginPassInfo = {};
		renderBeginPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderBeginPassInfo.renderPass = shadowFB.GetRenderPass();
		renderBeginPassInfo.framebuffer = shadowFB.GetFramebuffer();
		renderBeginPassInfo.renderArea.offset = { 0, 0 };
		renderBeginPassInfo.renderArea.extent = { 1024, 1024 };
		renderBeginPassInfo.clearValueCount = 1;
		renderBeginPassInfo.pClearValues = &depthClearValue;

		vkCmdBeginRenderPass(cmdBuffer, &renderBeginPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkBuffer vertexBuffers[] = { VK_NULL_HANDLE };
		VkDeviceSize offsets[] = { 0 };

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.GetPipeline());
		vertexBuffers[0] = model.GetVertexBuffer().GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuffer, model.GetIndexBuffer().GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &modelSet, 0, nullptr);
		vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(model.GetIndexCount()), 1, 0, 0, 0);

		vkCmdEndRenderPass(cmdBuffer);

		// OFFSCREEN

		// Set the normal camera
		ubo = (CameraUBO*)(((uint64_t)camerasData + (currentCamera * alignSingleUBOSize)));
		if (ubo)
		{
			ubo->model = modelMatrix;
			ubo->view = camera.GetViewMatrix();
			ubo->proj = camera.GetProjectionMatrix();
			ubo->proj[1][1] *= -1;
			ubo->lightSpaceMatrix = lightSpaceMatrix;
		}

		dynamicOffset = static_cast<uint32_t>(currentCamera) * alignSingleUBOSize;
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &globalBuffersSet, 1, &dynamicOffset);

		viewport.width = (float)surfaceExtent.width;
		viewport.height = (float)surfaceExtent.height;

		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
		
		VkClearValue clearValues[2] = {};
		clearValues[0].color = { 0.3f, 0.3f, 0.3f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderBeginPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderBeginPassInfo.renderPass = offscreenFB.GetRenderPass();
		renderBeginPassInfo.framebuffer = offscreenFB.GetFramebuffer();
		renderBeginPassInfo.renderArea.offset = { 0, 0 };
		renderBeginPassInfo.renderArea.extent = surfaceExtent;
		renderBeginPassInfo.clearValueCount = 2;
		renderBeginPassInfo.pClearValues = clearValues;

		vkCmdBeginRenderPass(cmdBuffer, &renderBeginPassInfo, VK_SUBPASS_CONTENTS_INLINE);	

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipeline.GetPipeline());
		vertexBuffers[0] = model.GetVertexBuffer().GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuffer, model.GetIndexBuffer().GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &modelSet, 0, nullptr);
		vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(model.GetIndexCount()), 1, 0, 0, 0);

		vertexBuffers[0] = floorModel.GetVertexBuffer().GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuffer, floorModel.GetIndexBuffer().GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &floorSet, 0, nullptr);
		vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(floorModel.GetIndexCount()), 1, 0, 0, 0);

		vertexBuffers[0] = quadVb.GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadStoragePipeline.GetPipeline());
		vkCmdDraw(cmdBuffer, 6, 1, 0, 0);


		// Render skybox as last
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline.GetPipeline());
		vertexBuffers[0] = skyboxVB.GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &skyboxSet, 0, nullptr);
		vkCmdDraw(cmdBuffer, 36, 1, 0, 0);

		// Particle system
		VkBuffer psVbs[] = { particleSystem.GetQuadVertexBuffer().GetBuffer(), particleSystem.GetInstancingBuffer().GetBuffer() };
		VkDeviceSize psVbsOffsets[] = { 0,0 };

		vkCmdBindVertexBuffers(cmdBuffer, 0, 2, psVbs, psVbsOffsets);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &psSet, 0, nullptr);
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, psPipeline.GetPipeline());
		vkCmdDraw(cmdBuffer, 6, particleSystem.GetNumAliveParticles(), 0, 0);


		vkCmdEndRenderPass(cmdBuffer);

		// Normal pass
		renderer.BeginDefaultRenderPass();

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postQuadPipeline.GetPipeline());
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &quadSet, 0, nullptr);
		vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

		renderer.EndDefaultRenderPass();
		renderer.EndCmdRecording();

		void* mapped = cameraUBO.Map(device, 0, VK_WHOLE_SIZE);
		memcpy(mapped, camerasData, static_cast<size_t>(currentCamera + 1) * alignSingleUBOSize);
		cameraUBO.Unmap(device);

		particleSystem.Update(deltaTime);
		const std::vector<ParticleInstanceData> &psData = particleSystem.GetInstanceData();

		VKBuffer& instancingBuffer = particleSystem.GetInstancingBuffer();
		mapped = instancingBuffer.Map(device, 0, VK_WHOLE_SIZE);
		memcpy(mapped, psData.data(), psData.size() * sizeof(ParticleInstanceData));
		instancingBuffer.Unmap(device);

		
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

	vkDestroyDescriptorSetLayout(device, buffersSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, texturesSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, userTexturesSetLayout, nullptr);

	quadStoragePipeline.Dispose(device);
	quadStorageShader.Dispose(device);
	quadVb.Dispose(device);

	cameraUBO.Dispose(device);
	offscreenFB.Dispose(device);
	shadowFB.Dispose(device);

	vkDestroyFence(device, computeFence, nullptr);
	storageTexture.Dispose(device);
	vkDestroyPipeline(device, computePipeline, nullptr);
	vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, computeSetLayout, nullptr);
	computeShader.Dispose(device);
	vkDestroySemaphore(device, computeSemaphore, nullptr);
	vkDestroySemaphore(device, graphicsSemaphore, nullptr);
	
	particleSystem.Dispose(device);

	cubemap.Dispose(device);
	modelTexture.Dispose(device);
	model.Dispose(device);
	floorTexture.Dispose(device);
	floorModel.Dispose(device);
	skyboxVB.Dispose(device);

	modelShader.Dispose(device);
	skyboxShader.Dispose(device);
	postQuadShader.Dispose(device);
	shadowShader.Dispose(device);
	psShader.Dispose(device);

	modelPipeline.Dispose(device);
	skyboxPipeline.Dispose(device);
	postQuadPipeline.Dispose(device);
	shadowPipeline.Dispose(device);
	psPipeline.Dispose(device);

	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

	renderer.Dispose();
	base.Dispose();

	glfwTerminate();

	return 0;
}