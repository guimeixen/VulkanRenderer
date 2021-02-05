#include "VKTexture3D.h"

#include <iostream>

VKTexture3D::VKTexture3D()
{
	width = 0;
	height = 0;
	depth = 0;
	mipLevels = 0;
	image = VK_NULL_HANDLE;
	imageView = VK_NULL_HANDLE;
	memory = VK_NULL_HANDLE;
	sampler = VK_NULL_HANDLE;
	params = {};
}

bool VKTexture3D::CreateFromData(VKBase& base, const TextureParams &params, unsigned int width, unsigned int height, unsigned int depth, const void* data)
{
	this->params = params;
	mipLevels = 1;
	this->width = width;
	this->height = height;
	this->depth = depth;

	if (params.useStorage)
	{
		// Make sure the format supports storage
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(base.GetPhysicalDevice(), params.format, &props);

		if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
		{
			std::cout << "Format requested for storage image doesn't support storage\n";
			return false;
		}
	}


	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_3D;
	imageCreateInfo.format = params.format;
	imageCreateInfo.extent.width = static_cast<uint32_t>(width);
	imageCreateInfo.extent.height = static_cast<uint32_t>(height);
	imageCreateInfo.extent.depth = static_cast<uint32_t>(depth);
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (data)
	{
		if (params.useStorage)
			imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		else
			imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	else
	{
		if (params.useStorage)
			imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		else
			imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	}

	VkDevice device = base.GetDevice();

	if (vkCreateImage(device, &imageCreateInfo, nullptr, &image) != VK_SUCCESS)
	{
		std::cout << "Failed to create image\n";
		return false;
	}

	VkMemoryRequirements imageMemReqs;
	vkGetImageMemoryRequirements(device, image, &imageMemReqs);

	VkMemoryAllocateInfo imgAllocInfo = {};
	imgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	imgAllocInfo.memoryTypeIndex = vkutils::FindMemoryType(base.GetPhysicalDeviceMemoryProperties(), imageMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imgAllocInfo.allocationSize = imageMemReqs.size;

	if (vkAllocateMemory(device, &imgAllocInfo, nullptr, &memory) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate image memory\n";
		return false;
	}

	vkBindImageMemory(device, image, memory, 0);

	unsigned int textureSize = width * height * depth * 4 * sizeof(unsigned char);

	VKBuffer stagingBuffer;
	stagingBuffer.Create(&base, textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* mapped;
	vkMapMemory(device, stagingBuffer.GetBufferMemory(), 0, static_cast<VkDeviceSize>(stagingBuffer.GetSize()), 0, &mapped);
	memcpy(mapped, data, static_cast<size_t>(stagingBuffer.GetSize()));
	vkUnmapMemory(device, stagingBuffer.GetBufferMemory());

	base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	base.CopyBufferToImage3D(stagingBuffer, image, width, height, depth);
	base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	stagingBuffer.Dispose(device);

	if (!CreateImageView(device, VK_IMAGE_ASPECT_COLOR_BIT))
		return false;
	if (!CreateSampler(device))
		return false;
}

void VKTexture3D::Dispose(VkDevice device)
{
	if (image != VK_NULL_HANDLE)
		vkDestroyImage(device, image, nullptr);
	if (imageView != VK_NULL_HANDLE)
		vkDestroyImageView(device, imageView, nullptr);
	if (sampler != VK_NULL_HANDLE)
		vkDestroySampler(device, sampler, nullptr);
	if (memory != VK_NULL_HANDLE)
		vkFreeMemory(device, memory, nullptr);
}

bool VKTexture3D::CreateImageView(VkDevice device, VkImageAspectFlags imageAspect)
{
	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = image;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
	imageViewInfo.format = params.format;
	imageViewInfo.subresourceRange.aspectMask = imageAspect;
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = static_cast<uint32_t>(mipLevels);
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(device, &imageViewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		std::cout << "Failed to create image view\n";
		return false;
	}

	return true;
}

bool VKTexture3D::CreateSampler(VkDevice device)
{
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = params.filter;
	samplerInfo.minFilter = params.filter;
	samplerInfo.addressModeU = params.addressMode;
	samplerInfo.addressModeV = params.addressMode;
	samplerInfo.addressModeW = params.addressMode;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = (float)mipLevels;

	if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
	{
		std::cout << "Failed to create sampler\n";
		return false;
	}

	return true;
}
