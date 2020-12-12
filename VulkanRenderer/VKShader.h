#pragma once

#include <vulkan/vulkan.h>

#include <string>

class VKShader
{
public:
	VKShader();

	void LoadShader(VkDevice device, const std::string &vertexPath, const std::string &fragmentPath);
	void Dispose(VkDevice device);

	VkShaderModule GetVertexShaderModule() const { return vertexModule; }
	VkShaderModule GetFragmentShaderModule() const { return fragmentModule; }

	const VkPipelineShaderStageCreateInfo& GetVertexStageInfo() const { return vertexStageInfo; }
	const VkPipelineShaderStageCreateInfo& GetFragmentStageInfo() const { return fragmentStageInfo; }

private:
	VkShaderModule vertexModule;
	VkShaderModule fragmentModule;
	VkPipelineShaderStageCreateInfo vertexStageInfo;
	VkPipelineShaderStageCreateInfo fragmentStageInfo;
};