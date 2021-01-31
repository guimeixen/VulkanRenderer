#include "Skybox.h"

#include "glm/glm.hpp"

#include <iostream>

Skybox::Skybox()
{
	set = VK_NULL_HANDLE;
}

bool Skybox::Load(VKRenderer& renderer, const std::vector<std::string>& facesPath, VkRenderPass renderPass)
{
	VKBase& base = renderer.GetBase();

	TextureParams cubemapParams = {};
	cubemapParams.format = VK_FORMAT_R8G8B8A8_SRGB;
	cubemapParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	cubemapParams.filter = VK_FILTER_LINEAR;

	cubeMap.LoadCubemapFromFiles(base, facesPath, cubemapParams);

	VkDevice device = base.GetDevice();

	set = renderer.AllocateUserTextureDescriptorSet();

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = cubeMap.GetImageView();
	imageInfo.sampler = cubeMap.GetSampler();

	VkWriteDescriptorSet descriptorWrites = {};
	descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites.dstBinding = 0;
	descriptorWrites.dstArrayElement = 0;
	descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites.descriptorCount = 1;
	descriptorWrites.dstSet = set;
	descriptorWrites.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);


	// Create the vertex buffer

	float vertices[] = {
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
	vertexStagingBuffer.Create(&base, sizeof(vertices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	unsigned int vertexSize = vertexStagingBuffer.GetSize();

	void* mapped = vertexStagingBuffer.Map(device, 0, vertexSize);
	memcpy(mapped, vertices, (size_t)vertexSize);
	vertexStagingBuffer.Unmap(device);

	vb.Create(&base, sizeof(vertices), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	base.CopyBuffer(vertexStagingBuffer, vb, vertexSize);

	vertexStagingBuffer.Dispose(device);


	// Create the skybox pipeline

	PipelineInfo pipeInfo = VKPipeline::DefaultFillStructs();

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = base.GetSurfaceExtent();

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
	};

	VkVertexInputBindingDescription bindingDesc = {};
	bindingDesc.binding = 0;
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc.stride = sizeof(glm::vec3);

	VkVertexInputAttributeDescription attribDesc = {};
	attribDesc.binding = 0;
	attribDesc.format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDesc.location = 0;
	attribDesc.offset = 0;

	// No need to set the viewport because it's dynamic
	pipeInfo.viewportState.viewportCount = 1;
	pipeInfo.viewportState.scissorCount = 1;
	pipeInfo.viewportState.pScissors = &scissor;

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	pipeInfo.dynamicState.dynamicStateCount = 1;
	pipeInfo.dynamicState.pDynamicStates = dynamicStates;

	pipeInfo.vertexInput.vertexBindingDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexBindingDescriptions = &bindingDesc;
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexAttributeDescriptions = &attribDesc;

	// Make sure to use LEQUAL in depth testing, because we set the skybox depth to 1 in the vertex shader
	// and otherwise wouldn't render if we kept using just LESS
	pipeInfo.depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	shader.LoadShader(device, "Data/Shaders/skybox_vert.spv", "Data/Shaders/skybox_frag.spv");

	if (!pipeline.Create(device, pipeInfo, renderer.GetPipelineLayout(), shader, renderPass))
		return false;

	return true;
}

void Skybox::Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout)
{
	VkBuffer vertexBuffers[] = { vb.GetBuffer() };
	VkDeviceSize offsets[] = { 0 };

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());	
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &set, 0, nullptr);
	vkCmdDraw(cmdBuffer, 36, 1, 0, 0);
}

void Skybox::Dispose(VkDevice device)
{
	cubeMap.Dispose(device);
	shader.Dispose(device);
	pipeline.Dispose(device);
	vb.Dispose(device);
}
