#pragma once

#include "VKBase.h"
#include "VKRenderer.h"
#include "VKPipeline.h"

#include "glm/glm.hpp"

struct Particle
{
	glm::vec3 pos;
	float life;
	glm::vec4 color;
	glm::vec3 velocity;
};

struct ParticleInstanceData
{
	glm::vec4 pos;
	glm::vec4 color;
};

class ParticleSystem
{
public:
	ParticleSystem();

	bool Init(VKRenderer &renderer, const std::string texturePath, unsigned int maxParticles);
	void Update(float dt);
	void Render(VkCommandBuffer cmdBuffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout);
	const std::vector<ParticleInstanceData>& GetInstanceData();
	void Dispose(VkDevice device);

	const VKTexture2D& GetTexture() const { return texture; }
	const VKBuffer& GetQuadVertexBuffer() const { return vb; }
	VKBuffer& GetInstancingBuffer() { return instancingBuffer; }
	unsigned int GetMaxParticles() const { return maxParticles; }
	unsigned int GetNumAliveParticles() const { return static_cast<unsigned int>(instanceData.size()); }

private:
	void RespawnParticle(Particle& p);
	int FirstUnusedParticle();

private:
	VKTexture2D texture;
	VKBuffer vb;
	VKBuffer instancingBuffer;
	VkDescriptorSet set;
	std::vector<Particle> particles;
	std::vector<ParticleInstanceData> instanceData;
	unsigned int maxParticles;
	unsigned int lastUsedParticle;
	float accumulator;
	float emission;
	float startLifeTime;
};

