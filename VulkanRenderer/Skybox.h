#pragma once

#include "VKRenderer.h"
#include "VKShader.h"
#include "VKPipeline.h"

class Skybox
{
public:
	Skybox();

	bool Load(VKRenderer* renderer, const std::vector<std::string>& facesPath, VkRenderPass renderPass);
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

