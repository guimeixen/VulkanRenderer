#include "MeshDefaults.h"

#include "VertexTypes.h"

Mesh MeshDefaults::CreateQuad(VKRenderer* renderer)
{
	Mesh m = {};
	
	VkVertexInputBindingDescription bindingDesc = {};
	bindingDesc.binding = 0;
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc.stride = sizeof(glm::vec4);

	m.bindings.push_back(bindingDesc);

	VkVertexInputAttributeDescription attribDesc = {};
	attribDesc.binding = 0;
	attribDesc.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attribDesc.location = 0;
	attribDesc.offset = 0;

	m.attribs.push_back(attribDesc);

	glm::vec4 quadVertices[] = {
		glm::vec4(-1.0f, -1.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f,	0.0f, 1.0f),
		glm::vec4(1.0f, -1.0f,	1.0f, 0.0f),

		glm::vec4(1.0f, -1.0f,	1.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f,	0.0f, 1.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
	};

	VKBase& base = renderer->GetBase();
	VkDevice device = renderer->GetBase().GetDevice();

	VKBuffer quadVertexStagingBuffer;
	quadVertexStagingBuffer.Create(&base, sizeof(quadVertices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	unsigned int vertexSize = quadVertexStagingBuffer.GetSize();

	void* mapped = quadVertexStagingBuffer.Map(device, 0, vertexSize);
	memcpy(mapped, quadVertices, (size_t)vertexSize);
	quadVertexStagingBuffer.Unmap(device);

	m.vb.Create(&base, sizeof(quadVertices), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	base.CopyBuffer(quadVertexStagingBuffer, m.vb, vertexSize);
	quadVertexStagingBuffer.Dispose(device);

	return m;
}
