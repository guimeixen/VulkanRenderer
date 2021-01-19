#include "VKBuffer.h"

#include "VKUtils.h"
#include "VKBase.h"

#include <iostream>

VKBuffer::VKBuffer()
{
	buffer = VK_NULL_HANDLE;
	bufferMemory = VK_NULL_HANDLE;
	memReqs = {};
	size = 0;
}

bool VKBuffer::Create(VKBase *base, unsigned int size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags)
{
	// UBO must be aligned
	if (usage == VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)
	{
		VkDeviceSize minUBOAlignment = base->GetPhysicalDeviceLimits().minUniformBufferOffsetAlignment;
		VkDeviceSize alignedUBOSize = (VkDeviceSize)size;

		if (minUBOAlignment > 0)
			alignedUBOSize = (alignedUBOSize + minUBOAlignment - 1) & ~(minUBOAlignment - 1);

		this->size = alignedUBOSize;
	}
	else
	{
		this->size = size;
	}

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = this->size;
	bufferCreateInfo.usage = usage;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;		// The buffer will only be used from the graphics queue so we can use exlusive. How would we do if we had a separate compute queue

	VkDevice device = base->GetDevice();

	if (vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS)
	{
		std::cout << "Failed to create vertex buffer\n";
		return false;
	}

	vkGetBufferMemoryRequirements(device, buffer, &memReqs);

	VkMemoryAllocateInfo bufAllocInfo = {};
	bufAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	bufAllocInfo.allocationSize = memReqs.size;
	bufAllocInfo.memoryTypeIndex = vkutils::FindMemoryType(base->GetPhysicalDeviceMemoryProperties(), memReqs.memoryTypeBits, memoryPropertyFlags);

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
