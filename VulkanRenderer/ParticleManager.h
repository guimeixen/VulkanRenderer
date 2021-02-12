#pragma once

#include "ParticleSystem.h"
#include "VKRenderer.h"

class ParticleManager
{
public:
	ParticleManager();

	bool Init(VKRenderer* renderer, VkRenderPass renderPass);
	bool AddParticleSystem(VKRenderer* renderer, const std::string texturePath, unsigned int maxParticles);
	void Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);
	void Update(VkDevice device, float dt);
	void Dispose(VkDevice device);

	const std::vector<ParticleSystem>& GetParticlesystems() const { return particleSystems; }

private:
	VKShader vertexShader;
	VKShader fragmentShader;
	VKPipeline pipeline;

	std::vector<ParticleSystem> particleSystems;
};

