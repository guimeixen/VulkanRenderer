#include "VKPipeline.h"

#include <iostream>

VKPipeline::VKPipeline()
{
	pipeline = VK_NULL_HANDLE;
}

bool VKPipeline::Create(VkDevice device, const PipelineInfo& info, VkPipelineLayout pipelineLayout, const VKShader& shader, VkRenderPass renderPass)
{
	VkPipelineShaderStageCreateInfo shaderStages[] = { shader.GetVertexStageInfo(), shader.GetFragmentStageInfo() };

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &info.vertexInput;
	pipelineInfo.pInputAssemblyState = &info.inputAssembly;
	pipelineInfo.pViewportState = &info.viewportState;
	pipelineInfo.pRasterizationState = &info.rasterizer;
	pipelineInfo.pMultisampleState = &info.multisampling;
	pipelineInfo.pDepthStencilState = &info.depthStencilState;
	pipelineInfo.pColorBlendState = &info.colorBlending;
	pipelineInfo.pDynamicState = &info.dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		std::cout << "Failed to create graphics pipeline\n";
		return false;
	}
	std::cout << "Create pipeline\n";

	return true;
}

void VKPipeline::Dispose(VkDevice device)
{
	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
	}
}

PipelineInfo VKPipeline::DefaultFillStructs()
{
	PipelineInfo info = {};

	info.vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	info.inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	info.inputAssembly.primitiveRestartEnable = VK_FALSE;

	info.viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;

	info.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.rasterizer.depthClampEnable = VK_FALSE;
	info.rasterizer.rasterizerDiscardEnable = VK_FALSE;
	info.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	info.rasterizer.lineWidth = 1.0f;
	info.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	info.rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	info.rasterizer.depthBiasEnable = VK_FALSE;
	info.rasterizer.depthBiasConstantFactor = 0.0f;
	info.rasterizer.depthBiasClamp = 0.0f;
	info.rasterizer.depthBiasSlopeFactor = 0.0f;

	info.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.multisampling.sampleShadingEnable = VK_FALSE;
	info.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	info.colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.colorBlending.logicOpEnable = VK_FALSE;
	info.colorBlending.blendConstants[0] = 0.0f;
	info.colorBlending.blendConstants[1] = 0.0f;
	info.colorBlending.blendConstants[2] = 0.0f;
	info.colorBlending.blendConstants[3] = 0.0f;

	info.depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	info.depthStencilState.depthTestEnable = VK_TRUE;
	info.depthStencilState.depthWriteEnable = VK_TRUE;
	info.depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
	info.depthStencilState.depthBoundsTestEnable = VK_FALSE;
	info.depthStencilState.stencilTestEnable = VK_FALSE;

	info.dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;

	return info;
}
