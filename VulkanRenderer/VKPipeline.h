#pragma once

#include "VKShader.h"

struct PipelineInfo
{
	VkPipelineVertexInputStateCreateInfo vertexInput;
	VkPipelineInputAssemblyStateCreateInfo inputAssembly;
	VkPipelineViewportStateCreateInfo viewportState;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo multisampling;
	VkPipelineColorBlendStateCreateInfo colorBlending;
	VkPipelineDepthStencilStateCreateInfo depthStencilState;
	VkPipelineDynamicStateCreateInfo dynamicState;
};

class VKPipeline
{
public:
	VKPipeline();
	
	bool Create(VkDevice device, const PipelineInfo& info, VkPipelineLayout pipelineLayout, const VKShader& vertexShader, const VKShader& fragmentShader, VkRenderPass renderPass);
	void Dispose(VkDevice device);

	VkPipeline GetPipeline() const { return pipeline; }

	static PipelineInfo DefaultFillStructs();

private:
	VkPipeline pipeline;
};

