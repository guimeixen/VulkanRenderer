#pragma once

#include "VKBase.h"

#include <string>

class Model
{
public:
	Model();
	bool Load(VKBase& base, const std::string &path);
	void Dispose(VkDevice device);

	unsigned int GetIndexCount() const { return indexCount; }
	const VKBuffer& GetVertexBuffer() const { return vertexBuffer; }
	const VKBuffer& GetIndexBuffer() const { return indexBuffer; }

private:
	unsigned int indexCount;
	VKBuffer vertexBuffer;
	VKBuffer indexBuffer;
};

