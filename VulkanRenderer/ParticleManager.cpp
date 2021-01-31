#include "ParticleManager.h"

#include <iostream>

ParticleManager::ParticleManager()
{
}

bool ParticleManager::Init(VKRenderer& renderer, VkRenderPass renderPass)
{
	VKBase& base = renderer.GetBase();
	// Create the particle system pipeline

	PipelineInfo pipeInfo = VKPipeline::DefaultFillStructs();

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = base.GetSurfaceExtent();

	// Additive blending
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
	};

	VkVertexInputBindingDescription bindingDesc[2] = {};
	bindingDesc[0].binding = 0;
	bindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc[0].stride = sizeof(glm::vec4);
	bindingDesc[1].binding = 1;
	bindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
	bindingDesc[1].stride = sizeof(ParticleInstanceData);

	VkVertexInputAttributeDescription attribDesc[3] = {};
	attribDesc[0].binding = 0;
	attribDesc[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribDesc[0].location = 0;
	attribDesc[0].offset = 0;

	attribDesc[1].binding = 1;
	attribDesc[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribDesc[1].location = 1;
	attribDesc[1].offset = 0;

	attribDesc[2].binding = 1;
	attribDesc[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribDesc[2].location = 2;
	attribDesc[2].offset = offsetof(ParticleInstanceData, color);

	// No need to set the viewport because it's dynamic
	pipeInfo.viewportState.viewportCount = 1;
	pipeInfo.viewportState.scissorCount = 1;
	pipeInfo.viewportState.pScissors = &scissor;

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	pipeInfo.dynamicState.dynamicStateCount = 1;
	pipeInfo.dynamicState.pDynamicStates = dynamicStates;

	pipeInfo.vertexInput.vertexBindingDescriptionCount = 2;
	pipeInfo.vertexInput.pVertexBindingDescriptions = bindingDesc;
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = 3;
	pipeInfo.vertexInput.pVertexAttributeDescriptions = attribDesc;

	// Don't write depth
	pipeInfo.depthStencilState.depthWriteEnable = VK_FALSE;

	shader.LoadShader(base.GetDevice(), "Data/Shaders/particle_vert.spv", "Data/Shaders/particle_frag.spv");

	if (!pipeline.Create(base.GetDevice(), pipeInfo, renderer.GetPipelineLayout(), shader, renderPass))
	{
		std::cout << "Failed to create particle system pipeline\n";
		return false;
	}

	return true;
}

bool ParticleManager::AddParticleSystem(VKRenderer& renderer, const std::string texturePath, unsigned int maxParticles)
{
	ParticleSystem particleSystem;
	if (!particleSystem.Init(renderer, texturePath, maxParticles))
		return false;

	particleSystems.push_back(particleSystem);

	return true;
}

void ParticleManager::Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout)
{
	for (size_t i = 0; i < particleSystems.size(); i++)
	{
		particleSystems[i].Render(cmdBuffer, pipeline.GetPipeline(), pipelineLayout);
	}
}

void ParticleManager::Update(VkDevice device, float dt)
{
	// TODO: Use one big buffer and offset into it

	for (size_t i = 0; i < particleSystems.size(); i++)
	{
		ParticleSystem &p = particleSystems[i];
		p.Update(dt);

		const std::vector<ParticleInstanceData>& psData = p.GetInstanceData();

		VKBuffer& instancingBuffer = p.GetInstancingBuffer();

		void* mapped = instancingBuffer.Map(device, 0, VK_WHOLE_SIZE);
		memcpy(mapped, psData.data(), psData.size() * sizeof(ParticleInstanceData));
		instancingBuffer.Unmap(device);
	}
}

void ParticleManager::Dispose(VkDevice device)
{
	for (size_t i = 0; i < particleSystems.size(); i++)
	{
		particleSystems[i].Dispose(device);
	}

	shader.Dispose(device);
	pipeline.Dispose(device);
}
