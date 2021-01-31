#include "ParticleSystem.h"

#include "Random.h"

#include <iostream>

ParticleSystem::ParticleSystem()
{
	maxParticles = 0;
	lastUsedParticle = 0;
	accumulator = 0.0f;
	emission = 8.0f;
	startLifeTime = 1.0f;
}

bool ParticleSystem::Init(VKRenderer& renderer, const std::string texturePath, unsigned int maxParticles)
{
	this->maxParticles = maxParticles;

	VKBase& base = renderer.GetBase();

	TextureParams textureParams = {};
	textureParams.format = VK_FORMAT_R8G8B8A8_SRGB;
	textureParams.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	textureParams.filter = VK_FILTER_LINEAR;

	if (!texture.LoadFromFile(base, texturePath, textureParams))
		return false;

	instanceData.resize(maxParticles);
	particles.resize(maxParticles);

	glm::vec4 vertices[] = {
		 glm::vec4(-1.0f, 1.0f,	0.0f, 1.0f),
		 glm::vec4(-1.0f, -1.0f, 0.0f, 0.0f),
		 glm::vec4(1.0f, -1.0f,	1.0f, 0.0f),
		 glm::vec4(-1.0f, 1.0f,	0.0f, 1.0f),
		 glm::vec4(1.0f, -1.0f,	1.0f, 0.0f),
		 glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
	};

	VkDevice device = base.GetDevice();

	VKBuffer vertexStagingBuffer;
	vertexStagingBuffer.Create(&base, sizeof(vertices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	unsigned int vertexSize = vertexStagingBuffer.GetSize();

	void* mapped = vertexStagingBuffer.Map(device, 0, vertexSize);
	memcpy(mapped, vertices, (size_t)vertexSize);
	vertexStagingBuffer.Unmap(device);

	vb.Create(&base, sizeof(vertices), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	base.CopyBuffer(vertexStagingBuffer, vb, vertexSize);
	vertexStagingBuffer.Dispose(device);


	unsigned int particlesBufferSize = maxParticles * sizeof(ParticleInstanceData);

	if (!instancingBuffer.Create(&base, particlesBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
		return false;


	if (particles.size() > 0)
	{
		RespawnParticle(particles[0]);		// Spawn one particle so they get update initially
	}

	set = renderer.AllocateUserTextureDescriptorSet();

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = texture.GetImageView();
	imageInfo.sampler = texture.GetSampler();

	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.dstSet = set;
	descriptorWrite.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

	return true;
}

void ParticleSystem::Update(float dt)
{
	for (unsigned int i = 0; i < maxParticles; i++)
	{
		Particle& p = particles[i];

		p.life -= dt / startLifeTime;

		if (p.life > 0.0f)
		{
			p.pos.y += 1.0f * dt;
		}
	}

	accumulator += dt;							// This line and the while are used to spawn newParticles (emission) per second and not per frame
	float denom = 1.0f / emission;

	while (accumulator > denom)
	{
		int unusedParticle = FirstUnusedParticle();
		if (unusedParticle == 0 && particles[0].life <= 0.0f)		// This is used in the case that there is only one particle to let its life reach 0
		{
			RespawnParticle(particles[0]);
		}
		else if (unusedParticle > 0)
		{
			RespawnParticle(particles[unusedParticle]);
		}
		accumulator -= denom;
	}
}

void ParticleSystem::Render(VkCommandBuffer cmdBuffer, VkPipeline pipeline, VkPipelineLayout pipelineLayout)
{
	VkBuffer vbs[] = { vb.GetBuffer(), instancingBuffer.GetBuffer() };
	VkDeviceSize vbsOffsets[] = { 0,0 };

	vkCmdBindVertexBuffers(cmdBuffer, 0, 2, vbs, vbsOffsets);
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &set, 0, nullptr);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
	vkCmdDraw(cmdBuffer, 6, GetNumAliveParticles(), 0, 0);
}

void ParticleSystem::RespawnParticle(Particle& p)
{
	p.pos.x = Random::Float();
	p.pos.y = Random::Float(1.0f, 2.0f);
	p.pos.z = Random::Float();
	p.life = startLifeTime;
	p.velocity = glm::vec3(0.0f);
	p.color.x = Random::Float();
	p.color.y = Random::Float();
	p.color.z = Random::Float();

	//std::cout << "Respawning\n";
}

int ParticleSystem::FirstUnusedParticle()
{
	for (unsigned int i = lastUsedParticle; i < maxParticles; ++i)
	{
		if (particles[i].life <= 0.0f)
		{
			lastUsedParticle = i;
			return i;
		}
	}

	for (int i = 0; i < lastUsedParticle; i++)
	{
		if (particles[i].life <= 0.0f)
		{
			lastUsedParticle = i;
			return i;
		}
	}

	lastUsedParticle = 0;
	return 0;
}

const std::vector<ParticleInstanceData>& ParticleSystem::GetInstanceData()
{
	instanceData.clear();

	for (unsigned int i = 0; i < maxParticles; i++)
	{
		Particle& p = particles[i];

		if (p.life > 0.0f)
		{
			ParticleInstanceData data;
			data.pos = glm::vec4(p.pos, 0.0f);
			data.color = p.color;

			instanceData.push_back(data);
		}
	}

	return instanceData;
}

void ParticleSystem::Dispose(VkDevice device)
{
	texture.Dispose(device);
	vb.Dispose(device);
	instancingBuffer.Dispose(device);
}
