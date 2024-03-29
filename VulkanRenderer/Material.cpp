#include "Material.h"

Material::Material()
{
}

bool Material::Create(VKRenderer* renderer, const Mesh &mesh, const MaterialFeatures& features, const std::string& vertexPath, const std::string& fragmentPath, VkRenderPass renderPass)
{
	VkDevice device = renderer->GetBase().GetDevice();

	if (!vertexShader.LoadShader(device, vertexPath, VK_SHADER_STAGE_VERTEX_BIT))
		return false;
	if (!fragmentShader.LoadShader(device, fragmentPath, VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	PipelineInfo pipeInfo = VKPipeline::DefaultFillStructs();

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = renderer->GetBase().GetSurfaceExtent();

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	if (features.enableBlend)
	{
		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	}

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
	};

	// No need to set the viewport because it's dynamic
	pipeInfo.viewportState.viewportCount = 1;
	pipeInfo.viewportState.scissorCount = 1;
	pipeInfo.viewportState.pScissors = &scissor;

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	pipeInfo.dynamicState.dynamicStateCount = 1;
	pipeInfo.dynamicState.pDynamicStates = dynamicStates;

	pipeInfo.depthStencilState.depthWriteEnable = features.enableDepthWrite;

	pipeInfo.rasterizer.frontFace = features.frontFace;
	pipeInfo.rasterizer.cullMode = features.cullMode;

	pipeInfo.vertexInput.vertexBindingDescriptionCount = (uint32_t)mesh.bindings.size();
	pipeInfo.vertexInput.pVertexBindingDescriptions = mesh.bindings.data();
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = (uint32_t)mesh.attribs.size();
	pipeInfo.vertexInput.pVertexAttributeDescriptions = mesh.attribs.data();

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	if (!pipeline.Create(device, pipeInfo, renderer->GetPipelineLayout(), vertexShader, fragmentShader, renderPass))
		return false;

	return true;
}

void Material::Dispose(VkDevice device)
{
	pipeline.Dispose(device);
	vertexShader.Dispose(device);
	fragmentShader.Dispose(device);
}
