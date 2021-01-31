#pragma once

#include "ParticleSystem.h"

class ParticleManager
{
public:
	ParticleManager();

	bool Init(VKBase& base, VkDescriptorPool descriptorPool, VkDescriptorSetLayout userTexturesSetLayout, VkPipelineLayout pipelineLayout, VkRenderPass renderPass);
	bool AddParticleSystem(VKBase& base, const std::string texturePath, unsigned int maxParticles);
	void Render(VkCommandBuffer cmdBuffer);
	void Update(VkDevice device, float dt);
	void Dispose(VkDevice device);

	const std::vector<ParticleSystem>& GetParticlesystems() const { return particleSystems; }

private:
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout userTexturesSetLayout;
	VkPipelineLayout pipelineLayout;

	VKShader shader;
	VKPipeline pipeline;

	std::vector<ParticleSystem> particleSystems;
};

