#pragma once

#include <vulkan/vulkan.h>

class VKBase;

class VKBuffer
{
public:
	VKBuffer();

	bool Create(VKBase *base, unsigned int size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags);
	void Dispose(VkDevice device);

	void *Map(VkDevice device, VkDeviceSize offset, VkDeviceSize size);
	void Unmap(VkDevice device);

	VkBuffer GetBuffer() const { return buffer; }
	VkDeviceMemory GetBufferMemory() const { return bufferMemory; }
	VkMemoryRequirements GetMemoryRequirements() const { return memReqs; }
	unsigned int GetSize() const { return size; }

private:
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;
	VkMemoryRequirements memReqs;
	unsigned int size;
	void* mapped;
};