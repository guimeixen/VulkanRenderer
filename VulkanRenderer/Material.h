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
	void Dispose(VkDevice device);

	VkPipeline GetPipeline() const { return pipeline.GetPipeline(); }

private:
	VKShader shader;
	VKPipeline pipeline;
};

