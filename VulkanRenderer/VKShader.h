#pragma once

#include <vulkan/vulkan.h>

#include <string>

class VKShader
{
public:
	VKShader();

	bool LoadShader(VkDevice device, const std::string &vertexPath, const std::string &fragmentPath);
	bool LoadShader(VkDevice device, const std::string &computePath);
	void Dispose(VkDevice device);

	const VkPipelineShaderStageCreateInfo& GetVertexStageInfo() const { return vertexStageInfo; }
	const VkPipelineShaderStageCreateInfo& GetFragmentStageInfo() const { return fragmentStageInfo; }
	const VkPipelineShaderStageCreateInfo& GetComputeStageInfo() const { return computeStageInfo; }

private:
	VkShaderModule vertexModule;
	VkShaderModule fragmentModule;
	VkShaderModule computeModule;
	VkPipelineShaderStageCreateInfo vertexStageInfo;
	VkPipelineShaderStageCreateInfo fragmentStageInfo;
	VkPipelineShaderStageCreateInfo computeStageInfo;
};