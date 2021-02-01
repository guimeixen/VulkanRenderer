#pragma once

#include "Model.h"
#include "VKShader.h"
#include "VKPipeline.h"
#include "VKRenderer.h"

#include "glm/glm.hpp"

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

	bool Init(VKRenderer &renderer, VkRenderPass renderPass);
	bool AddModel(VKRenderer& renderer, const std::string &path, const std::string &texturePath);
	void Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout, VkPipeline shadowMapPipeline);
	void Dispose(VkDevice device);

	const std::vector<RenderModel>& GetRenderModels() const { return models; }
	const std::vector<glm::mat4>& GetModelsMatrices() const { return modelMatrices; }

private:
	std::vector<RenderModel> models;
	std::vector<glm::mat4> modelMatrices;

	unsigned int instanceDataOffset;

	VKShader shader;
	VKPipeline pipeline;
};

