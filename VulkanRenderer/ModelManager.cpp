#include "ModelManager.h"
#include "VertexTypes.h"

#include <iostream>

ModelManager::ModelManager()
{
	instanceDataOffset = 0;
}

bool ModelManager::Init(VKRenderer& renderer, VkRenderPass renderPass)
{
	// Create the pipeline

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


	VKBase& base = renderer.GetBase();

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


	shader.LoadShader(base.GetDevice(), "Data/Shaders/shader_vert.spv", "Data/Shaders/shader_frag.spv");

	if (!pipeline.Create(base.GetDevice(), pipeInfo, renderer.GetPipelineLayout(), shader, renderPass))
	{
		std::cout << "Failed to create model pipeline\n";
		return false;
	}

	return true;
}

bool ModelManager::AddModel(VKRenderer& renderer, Entity e, const std::string& path, const std::string& texturePath)
{
	// Return the model and don't add a new entry if this entity already has a model
	if (map.find(e.id) != map.end())
	{
		//return GetModel(e);
		return true;
	}

	VKBase& base = renderer.GetBase();

	RenderModel renderModel = {};

	if (renderModel.model.Load(base, path) == false)
	{
		std::cout << "Failed to load model: " << path << '\n';
		return false;
	}

	TextureParams textureParams = {};
	textureParams.format = VK_FORMAT_R8G8B8A8_SRGB;
	textureParams.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	textureParams.filter = VK_FILTER_LINEAR;

	if (renderModel.texture.LoadFromFile(base, texturePath, textureParams) == false)
	{
		std::cout << "Failed to load texture: " << texturePath << '\n';
		return false;
	}

	VkDevice device = base.GetDevice();

	renderModel.set = renderer.AllocateUserTextureDescriptorSet();

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = renderModel.texture.GetImageView();
	imageInfo.sampler = renderModel.texture.GetSampler();

	VkWriteDescriptorSet descriptorWrites = {};
	descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrites.dstSet = renderModel.set;
	descriptorWrites.dstBinding = 0;
	descriptorWrites.dstArrayElement = 0;
	descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrites.descriptorCount = 1;
	descriptorWrites.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device, 1, &descriptorWrites, 0, nullptr);

	ModelInstance mi = {};
	mi.e = e;
	mi.renderModel = renderModel;

	InsertModelInstance(mi);

	return true;
}

void ModelManager::Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout, VkPipeline shadowMapPipeline)
{
	if (shadowMapPipeline != VK_NULL_HANDLE)
	{
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMapPipeline);
	}
	else
	{
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());
	}

	VkBuffer vertexBuffers[] = { VK_NULL_HANDLE };
	VkDeviceSize offsets[] = { 0 };

	for (size_t i = 0; i < models.size(); i++)
	{
		const Model& m = models[i].renderModel.model;

		vertexBuffers[0] = m.GetVertexBuffer().GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuffer, m.GetIndexBuffer().GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		vkCmdPushConstants(cmdBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(unsigned int), &instanceDataOffset);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &models[i].renderModel.set, 0, nullptr);
		vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(m.GetIndexCount()), 1, 0, 0, 0);

		instanceDataOffset += 1;
	}

	instanceDataOffset = 0;
}

void ModelManager::Dispose(VkDevice device)
{
	for (size_t i = 0; i < models.size(); i++)
	{
		models[i].renderModel.model.Dispose(device);
		models[i].renderModel.texture.Dispose(device);
	}

	shader.Dispose(device);
	pipeline.Dispose(device);
}

const RenderModel& ModelManager::GetRenderModel(Entity e) const
{
	return models[map.at(e.id)].renderModel;
}

void ModelManager::InsertModelInstance(const ModelInstance& mi)
{
	models.push_back(mi);
	map[mi.e.id] = models.size() - 1;
}
