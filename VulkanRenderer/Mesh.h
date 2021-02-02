#pragma once

#include "VKBuffer.h"

#include <vector>

struct Mesh
{
	std::vector<VkVertexInputBindingDescription> bindings;
	std::vector<VkVertexInputAttributeDescription> attribs;
	VKBuffer vb;
	unsigned int vertexCount;
	unsigned int vertexOffset;
	unsigned int indexCount;
	unsigned int indexOffset;
	unsigned int instanceCount;
	unsigned int instanceOffset;
};