#include "ModelManager.h"

#include <iostream>

ModelManager::ModelManager()
{
}

void ModelManager::Init(VKBase& base, VkDescriptorPool descriptorPool, VkDescriptorSetLayout userTexturesSetLayout)
{
	this->descriptorPool = descriptorPool;
	this->userTexturesSetLayout = userTexturesSetLayout;
}

bool ModelManager::AddModel(VKBase& base, const std::string& path, const std::string& texturePath)
{
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

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = descriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &userTexturesSetLayout;

	if (vkAllocateDescriptorSets(device, &setAllocInfo, &renderModel.set) != VK_SUCCESS)
	{
		std::cout << "failed to allocate descriptor sets\n";
		return 1;
	}

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

	models.push_back(renderModel);

	return true;
}

void ModelManager::Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout)
{
	VkBuffer vertexBuffers[] = { VK_NULL_HANDLE };
	VkDeviceSize offsets[] = { 0 };

	for (size_t i = 0; i < models.size(); i++)
	{
		const Model& m = models[i].model;

		vertexBuffers[0] = m.GetVertexBuffer().GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(cmdBuffer, m.GetIndexBuffer().GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &models[i].set, 0, nullptr);
		vkCmdDrawIndexed(cmdBuffer, static_cast<uint32_t>(m.GetIndexCount()), 1, 0, 0, 0);
	}
}

void ModelManager::Dispose(VkDevice device)
{
	for (size_t i = 0; i < models.size(); i++)
	{
		models[i].model.Dispose(device);
		models[i].texture.Dispose(device);
	}
}
