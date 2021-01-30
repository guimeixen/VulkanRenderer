#pragma once

#include "Model.h"
#include "VKTexture2D.h"

struct RenderModel
{
	Model model;
	VKTexture2D texture;
	VkDescriptorSet set;
};

class ModelManager
{
public:
	ModelManager();

	void Init(VKBase& base, VkDescriptorPool descriptorPool, VkDescriptorSetLayout userTexturesSetLayout);
	bool AddModel(VKBase &base, const std::string &path, const std::string &texturePath);
	void Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);
	void Dispose(VkDevice device);

	const std::vector<RenderModel>& GetRenderModels() const { return models; }

private:
	std::vector<RenderModel> models;
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout userTexturesSetLayout;
};

