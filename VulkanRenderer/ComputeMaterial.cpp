#include "ComputeMaterial.h"

#include <iostream>

ComputeMaterial::ComputeMaterial()
{
	pipeline = VK_NULL_HANDLE;
	pipelineLayout = VK_NULL_HANDLE;
	setLayout = VK_NULL_HANDLE;
}

bool ComputeMaterial::Create(VKRenderer& renderer, const std::string& computePath)
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

	if (vkCreateDescriptorSetLayout(device, &computeSetLayoutInfo, nullptr, &setLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create descriptor set layout\n";
		return false;
	}

	VkPipelineLayoutCreateInfo computePipeLayoutInfo = {};
	computePipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	computePipeLayoutInfo.setLayoutCount = 1;
	computePipeLayoutInfo.pSetLayouts = &setLayout;

	if (vkCreatePipelineLayout(device, &computePipeLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		std::cout << "Failed to create pipeline layout\n";
		return false;
	}

	if (!shader.LoadShader(device, "Data/Shaders/compute.spv"))
	{
		std::cout << "Failed to create compute shader\n";
		return false;
	}

	VkPipelineShaderStageCreateInfo computeStageInfo = shader.GetComputeStageInfo();

	VkComputePipelineCreateInfo computePipeInfo = {};
	computePipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipeInfo.layout = pipelineLayout;
	computePipeInfo.stage = computeStageInfo;

	if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipeInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		std::cout << "Failed to create compute pipeline\n";
		return false;
	}

	return true;
}

void ComputeMaterial::Dispose(VkDevice device)
{
	shader.Dispose(device);

	if (pipeline != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, pipeline, nullptr);
	}
	if (setLayout != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
	}
	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	}
}
