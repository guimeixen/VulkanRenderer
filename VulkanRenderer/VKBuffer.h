#pragma once

#include <vulkan/vulkan.h>

class VKBuffer
{
public:
	VKBuffer();

	bool Create(VkDevice device, VkPhysicalDeviceMemoryProperties memProperties, unsigned int size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags);
	void Dispose(VkDevice device);

	VkBuffer GetBuffer() const { return buffer; }
	VkDeviceMemory GetBufferMemory() const { return bufferMemory; }
	VkMemoryRequirements GetMemoryRequirements() const { return memReqs; }
	unsigned int GetSize() const { return size; }

private:
	VkBuffer buffer;
	VkDeviceMemory bufferMemory;
	VkMemoryRequirements memReqs;
	unsigned int size;
};