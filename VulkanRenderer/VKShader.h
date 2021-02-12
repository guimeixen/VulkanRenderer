#pragma once

#include <vulkan/vulkan.h>

#include <string>

class VKShader
{
public:
	VKShader();

	bool LoadShader(VkDevice device, const std::string &shaderName, VkShaderStageFlagBits shaderStage);
	void Dispose(VkDevice device);

	const VkPipelineShaderStageCreateInfo& GetStageInfo() const { return stageInfo; }

private:
	bool Compile(const std::string& baseShaderPath, const std::string& compiledShaderPath, VkShaderStageFlagBits stage);

private:
	VkShaderModule shaderModule;
	VkPipelineShaderStageCreateInfo stageInfo;

	bool compiled;
	bool compiledShaderFileExists;
	bool shaderNeedsCompile;
};