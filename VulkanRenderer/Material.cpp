#include "Material.h"

#include <iostream>

Material::Material()
{
}

bool Material::Create(VKRenderer& renderer, const Mesh &mesh, const MaterialFeatures& features, const std::string& vertexPath, const std::string& fragmentPath, VkRenderPass renderPass)
{
	VkDevice device = renderer.GetBase().GetDevice();

	shader.LoadShader(device, vertexPath, fragmentPath);

	PipelineInfo pipeInfo = VKPipeline::DefaultFillStructs();

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = renderer.GetBase().GetSurfaceExtent();

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
	};

	// No need to set the viewport because it's dynamic
	pipeInfo.viewportState.viewportCount = 1;
	pipeInfo.viewportState.scissorCount = 1;
	pipeInfo.viewportState.pScissors = &scissor;

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	pipeInfo.dynamicState.dynamicStateCount = 1;
	pipeInfo.dynamicState.pDynamicStates = dynamicStates;

	pipeInfo.depthStencilState.depthWriteEnable = features.enableDepthWrite;

	pipeInfo.rasterizer.frontFace = features.frontFace;
	pipeInfo.rasterizer.cullMode = features.cullMode;

	pipeInfo.vertexInput.vertexBindingDescriptionCount = (uint32_t)mesh.bindings.size();
	pipeInfo.vertexInput.pVertexBindingDescriptions = mesh.bindings.data();
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = (uint32_t)mesh.attribs.size();
	pipeInfo.vertexInput.pVertexAttributeDescriptions = mesh.attribs.data();

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	if (!pipeline.Create(device, pipeInfo, renderer.GetPipelineLayout(), shader, renderPass))
		return 1;

	return true;
}

bool Material::Create(VKRenderer& renderer, const std::string& computePath)
{
	VkDevice device = renderer.GetBase().GetDevice();

	VkDescriptorSetLayoutBinding computeSetLayoutBinding = {};
	computeSetLayoutBinding.binding = 0;
	computeSetLayoutBinding.descriptorCount = 1;
	computeSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	computeSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo computeSetLayoutInfo = {};
	computeSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	computeSetLayoutInfo.bindingCount = 1;
	computeSetLayoutInfo.pBindings = &computeSetLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &computeSetLayoutInfo, nullptr, &computeSetLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor set layout\n";
		return 1;
	}

	VkPipelineLayoutCreateInfo computePipeLayoutInfo = {};
	computePipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computePipeLayoutInfo.setLayoutCount = 1;
	computePipeLayoutInfo.pSetLayouts = &computeSetLayout;

	if (vkCreatePipelineLayout(device, &computePipeLayoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create pipeline layout\n";
		return 1;
	}

	if (!shader.LoadShader(device, "Data/Shaders/compute.spv"))
	{
		std::cout << "Failed to create compute shader\n";
		return 1;
	}

	VkPipelineShaderStageCreateInfo computeStageInfo = shader.GetComputeStageInfo();

	VkComputePipelineCreateInfo computePipeInfo = {};
	computePipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipeInfo.layout = computePipelineLayout;
	computePipeInfo.stage = computeStageInfo;

	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipeInfo, nullptr, &comPipeline) != VK_SUCCESS)
	{
		std::cout << "Failed to create compute pipeline\n";
		return 1;
	}

	return true;
}

void Material::Dispose(VkDevice device)
{
	pipeline.Dispose(device);
	shader.Dispose(device);
}
