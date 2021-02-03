#pragma once

#include "VKRenderer.h"
#include "VKShader.h"

class ComputeMaterial
{
public:
	ComputeMaterial();

	bool Create(VKRenderer& renderer, const std::string& computePath);
	void Dispose(VkDevice device);

	VkPipeline GetPipeline() const { return pipeline; }
	VkDescriptorSetLayout GetSetLayout() const { return setLayout; }
	VkPipelineLayout GetPipelineLayout() const { return pipelineLayout; }

private:
	VKShader shader;
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkDescriptorSetLayout setLayout;
};

