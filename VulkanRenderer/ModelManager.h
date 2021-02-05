#pragma once

#include "Model.h"
#include "VKShader.h"
#include "VKPipeline.h"
#include "VKRenderer.h"
#include "EntityManager.h"

#include "glm/glm.hpp"

#include <unordered_map>

struct RenderModel
{
	Model model;
	VKTexture2D texture;
	VkDescriptorSet set;
};

struct ModelInstance
{
	Entity e;
	RenderModel renderModel;
};

class ModelManager
{
public:
	ModelManager();

	bool Init(VKRenderer* renderer, VkRenderPass renderPass);
	bool AddModel(VKRenderer* renderer, Entity e, const std::string &path, const std::string &texturePath);
	void Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout, VkPipeline shadowMapPipeline);
	void Dispose(VkDevice device);

	const RenderModel& GetRenderModel(Entity e) const;

	unsigned int GetNumModels() const { return static_cast<unsigned int>(models.size()); }
	const std::vector<ModelInstance>& GetModelInstances() const { return models; }

private:
	void InsertModelInstance(const ModelInstance &instance);

private:
	std::vector<ModelInstance> models;
	std::unordered_map<unsigned int, unsigned int> map;

	unsigned int instanceDataOffset;

	VKShader shader;
	VKPipeline pipeline;
};

