#include "VKBuffer.h"

#include "VKUtils.h"

#include <iostream>

VKBuffer::VKBuffer()
{
	buffer = VK_NULL_HANDLE;
	memReqs = {};
	size = 0;
}

bool VKBuffer::Create(VkDevice device, VkPhysicalDeviceMemoryProperties memProperties, unsigned int size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags)
{
	this->size = size;

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = usage;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// The buffer will only be used from the graphics queue so we can use exlusive. How would we do if we had a separate compute queue

	if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		std::cout << "Failed to create vertex buffer\n";
		return false;
	}

	vkGetBufferMemoryRequirements(device, buffer, &memReqs);

	VkMemoryAllocateInfo bufAllocInfo = {};
	bufAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	bufAllocInfo.allocationSize = memReqs.size;
	bufAllocInfo.memoryTypeIndex = vkutils::FindMemoryType(memProperties, memReqs.memoryTypeBits, memoryPropertyFlags);

	if (vkAllocateMemory(device, &bufAllocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate vertex buffer memory\n";
		return false;
	}

	vkBindBufferMemory(device, buffer, bufferMemory, 0);

	return true;
}

void VKBuffer::Dispose(VkDevice device)
{
	if (buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(device, buffer, nullptr);
	if (bufferMemory != VK_NULL_HANDLE)
		vkFreeMemory(device, bufferMemory, nullptr);
}
