#pragma once

#include "VKTexture2D.h"
#include "VKShader.h"
#include "VKPipeline.h"

class Skybox
{
public:
	Skybox();

	bool Load(VKBase &base, const std::vector<std::string>& facesPath, VkDescriptorPool descriptorPool, VkDescriptorSetLayout userTexturesSetLayout, VkPipelineLayout pipelineLayout, VkRenderPass renderPass);
	void Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);
	void Dispose(VkDevice device);

	VkDescriptorSet GetDescriptorSet() const { return set; }

private:
	VKTexture2D cubeMap;
	VKBuffer vb;
	VkDescriptorSet set;
	VKShader shader;
	VKPipeline pipeline;
};

