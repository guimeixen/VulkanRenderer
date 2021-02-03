#include "Material.h"

Material::Material()
{
}

bool Material::Create(VKRenderer& renderer, const Mesh &mesh, const MaterialFeatures& features, const std::string& vertexPath, const std::string& fragmentPath, VkRenderPass renderPass)
{
	VkDevice device = renderer.GetBase().GetDevice();

	shader.LoadShader(device, vertexPath, fragmentPath);

	PipelineInfo pipeInfo = VKPipeline::DefaultFillStructs();

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = renderer.GetBase().GetSurfaceExtent();

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

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

	if (!pipeline.Create(device, pipeInfo, renderer.GetPipelineLayout(), shader, renderPass))
		return 1;

	return true;
}

void Material::Dispose(VkDevice device)
{
	pipeline.Dispose(device);
	shader.Dispose(device);
}
