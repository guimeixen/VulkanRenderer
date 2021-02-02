#pragma once

#include "VKRenderer.h"
#include "VKPipeline.h"
#include "Mesh.h"

struct MaterialFeatures
{
	VkBool32 enableDepthWrite;
	VkFrontFace frontFace;
	VkCullModeFlags cullMode;
};

class Material
{
public:
	Material();

	bool Create(VKRenderer& renderer, const Mesh &mesh, const MaterialFeatures &features, const std::string& vertexPath, const std::string& fragmentPath, VkRenderPass renderPass);
	bool Create(VKRenderer& renderer, const std::string& computePath);
	void Dispose(VkDevice device);


	VkPipeline GetPipeline() const { return pipeline.GetPipeline(); }
	VkPipeline GetComputePipeline() const { return comPipeline; }
	VkDescriptorSetLayout GetComputeSetLayout() const { return computeSetLayout; }
	VkPipelineLayout GetComputePipelineLayout() const { return computePipelineLayout; }

private:
	VKShader shader;
	VKPipeline pipeline;
	VkPipeline comPipeline;
	VkPipelineLayout computePipelineLayout;
	VkDescriptorSetLayout computeSetLayout;
};

