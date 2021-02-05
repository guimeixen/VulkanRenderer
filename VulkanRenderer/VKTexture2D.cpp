#include "VKTexture2D.h"

#include "stb_image.h"

#include <iostream>

VKTexture2D::VKTexture2D()
{
	width = 0;
	height = 0;
	mipLevels = 0;
	image = VK_NULL_HANDLE;
	imageView = VK_NULL_HANDLE;
	memory = VK_NULL_HANDLE;
	sampler = VK_NULL_HANDLE;
	params = {};
}

bool VKTexture2D::LoadFromFile(VKBase& base, const std::string& path, const TextureParams& textureParams)
{
	params = textureParams;
	textureType = TextureType::TEXTURE_2D;

	unsigned char* pixels = nullptr;
	int textureWidth, textureHeight, channels;
	pixels = stbi_load(path.c_str(), &textureWidth, &textureHeight, &channels, STBI_rgb_alpha);

	if (!pixels)
	{
		std::cout << "Failed to load texture: " << path << '\n';
		return false;
	}

	width = static_cast<unsigned int>(textureWidth);
	height = static_cast<unsigned int>(textureHeight);

	unsigned int textureSize = width * height * 4 * sizeof(unsigned char);

	if (params.dontCreateMipMaps)
		mipLevels = 1;
	else
		mipLevels = std::floor(std::log2(std::max(width, height))) + 1;

	// Check if the format supports image blit
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(base.GetPhysicalDevice(), params.format, &formatProperties);
	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) || !(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT))
	{
		std::cout << "Format doesn't support image blit\n";
		return false;
	}

	VKBuffer stagingBuffer;

	VkDevice device = base.GetDevice();

	stagingBuffer.Create(&base, textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* data;
	vkMapMemory(device, stagingBuffer.GetBufferMemory(), 0, static_cast<VkDeviceSize>(stagingBuffer.GetSize()), 0, &data);
	memcpy(data, pixels, static_cast<size_t>(stagingBuffer.GetSize()));
	vkUnmapMemory(device, stagingBuffer.GetBufferMemory());

	if (!CreateImage(device))
		return false;

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

	if (mipLevels == 1)
	{
		base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		base.CopyBufferToImage(stagingBuffer, image, textureWidth, textureHeight);
		base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	else
	{
		// Put the image ready for the image blits for mip map gen by transitioning to SRC_OPTIMAL
		base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		base.CopyBufferToImage(stagingBuffer, image, textureWidth, textureHeight);
		base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	}

	stagingBuffer.Dispose(device);

	if (!CreateImageView(device, VK_IMAGE_ASPECT_COLOR_BIT))
		return false;
	if (!CreateSampler(device))
		return false;

	return true;
}

bool VKTexture2D::LoadCubemapFromFiles(VKBase& base, const std::vector<std::string>& facesPath, const TextureParams& textureParams)
{
	params = textureParams;
	textureType = TextureType::TEXTURE_CUBE;

	unsigned char* facesPixels[6] = {};
	int textureWidth = 0;
	int textureHeight = 0;
	int channels = 0;
	int previousWidth = 0;
	int previouesHeight = 0;

	for (size_t i = 0; i < 6; i++)
	{
		// Make sure all faces have the same dimensions
		if (textureWidth != previousWidth)
		{
			std::cout << "Cubemap face " << i << " width different than the others\n";
			return false;
		}
		if (textureHeight != previouesHeight)
		{
			std::cout << "Cubemap face " << i << " height different than the others\n";
			return false;
		}

		facesPixels[i] = stbi_load(facesPath[i].c_str(), &textureWidth, &textureHeight, &channels, STBI_rgb_alpha);

		if (!facesPixels[i])
		{
			std::cout << "Failed to load cubemap face: " << facesPath[i] << '\n';
			return false;
		}

		previousWidth = textureWidth;
		previouesHeight = textureHeight;
	}

	width = static_cast<unsigned int>(textureWidth);
	height = static_cast<unsigned int>(textureHeight);

	unsigned int textureSize = width * height * 4 * 6 * sizeof(unsigned char);

	mipLevels = 1;

	VKBuffer stagingBuffer;

	VkDevice device = base.GetDevice();

	stagingBuffer.Create(&base, textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	void* data;
	vkMapMemory(device, stagingBuffer.GetBufferMemory(), 0, static_cast<VkDeviceSize>(stagingBuffer.GetSize()), 0, &data);

	unsigned int offset = 0;
	unsigned int increase = width * height * 4 * sizeof(unsigned char);		// Increase by the size of one texture every loop iteration
	for (size_t i = 0; i < 6; i++)
	{
		void* ptr = (char*)data + offset;
		memcpy(ptr, facesPixels[i], static_cast<size_t>(increase));
		offset += increase;
	}

	vkUnmapMemory(device, stagingBuffer.GetBufferMemory());

	if (!CreateImage(device))
		return false;

	VkMemoryRequirements imageMemReqs;
	vkGetImageMemoryRequirements(device, image, &imageMemReqs);

	VkMemoryAllocateInfo imgAllocInfo = {};
	imgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	imgAllocInfo.memoryTypeIndex = vkutils::FindMemoryType(base.GetPhysicalDeviceMemoryProperties(), imageMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imgAllocInfo.allocationSize = imageMemReqs.size;

	if (vkAllocateMemory(device, &imgAllocInfo, nullptr, &memory) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate cubemap image memory\n";
		return false;
	}

	vkBindImageMemory(device, image, memory, 0);

	base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6);
	base.CopyBufferToCubemapImage(stagingBuffer, image, textureWidth, textureHeight);
	base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6);

	stagingBuffer.Dispose(device);

	if (!CreateImageView(device, VK_IMAGE_ASPECT_COLOR_BIT))
		return false;
	if (!CreateSampler(device))
		return false;

	return true;
}

void VKTexture2D::Dispose(VkDevice device)
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

bool VKTexture2D::CreateImage(VkDevice device)
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.arrayLayers = 1;
	imageInfo.extent.width = static_cast<uint32_t>(width);
	imageInfo.extent.height = static_cast<uint32_t>(height);
	imageInfo.extent.depth = 1;
	imageInfo.format = params.format;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.mipLevels = static_cast<uint32_t>(mipLevels);
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;

	if (mipLevels > 1)
	{
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	else
	{
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	}


	if (textureType == TextureType::TEXTURE_CUBE)
	{
		imageInfo.arrayLayers = 6;
		imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	}

	if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
	{
		std::cout << "Failed to create image for texture\n";
		return false;
	}

	return true;
}

bool VKTexture2D::CreateImageView(VkDevice device, VkImageAspectFlags imageAspect)
{
	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.image = image;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = params.format;
	imageViewInfo.subresourceRange.aspectMask = imageAspect;
	imageViewInfo.subresourceRange.baseMipLevel = 0;
	imageViewInfo.subresourceRange.levelCount = static_cast<uint32_t>(mipLevels);
	imageViewInfo.subresourceRange.baseArrayLayer = 0;
	imageViewInfo.subresourceRange.layerCount = 1;

	if (textureType == TextureType::TEXTURE_CUBE)
	{
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		imageViewInfo.subresourceRange.layerCount = 6;
	}

	if (vkCreateImageView(device, &imageViewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		std::cout << "Failed to create image view\n";
		return false;
	}

	return true;
}

bool VKTexture2D::CreateSampler(VkDevice device)
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

bool VKTexture2D::CreateDepthTexture(const VKBase& base, const TextureParams& textureParams, unsigned int width, unsigned int height, bool sampled)
{
	params = textureParams;
	textureType = TextureType::TEXTURE_2D;
	mipLevels = 1;
	this->width = width;
	this->height = height;

	VkImageCreateInfo depthImageInfo = {};
	depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthImageInfo.arrayLayers = 1;
	depthImageInfo.extent.width = static_cast<uint32_t>(width);
	depthImageInfo.extent.height = static_cast<uint32_t>(height);
	depthImageInfo.extent.depth = 1;
	depthImageInfo.format = params.format;
	depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
	depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthImageInfo.mipLevels = 1;
	depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthImageInfo.flags = 0;
	depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	if (sampled)
		depthImageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

	VkDevice device = base.GetDevice();

	if (vkCreateImage(device, &depthImageInfo, nullptr, &image) != VK_SUCCESS)
	{
		std::cout << "Failed to create depth image\n";
		return false;
	}

	VkMemoryRequirements depthImageMemReqs;
	vkGetImageMemoryRequirements(device, image, &depthImageMemReqs);

	VkMemoryAllocateInfo imgAllocInfo = {};
	imgAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	imgAllocInfo.memoryTypeIndex = vkutils::FindMemoryType(base.GetPhysicalDeviceMemoryProperties(), depthImageMemReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	imgAllocInfo.allocationSize = depthImageMemReqs.size;

	if (vkAllocateMemory(device, &imgAllocInfo, nullptr, &memory) != VK_SUCCESS)
	{
		std::cout << "Failed to allocate depth image memory\n";
		return 1;
	}

	vkBindImageMemory(device, image, memory, 0);

	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (vkutils::FormatHasStencil(params.format))
		aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;

	if (!CreateImageView(device, aspect))
		return false;

	if (sampled)
	{
		if (!CreateSampler(device))
			return false;
	}

	return true;
}

bool VKTexture2D::CreateColorTexture(const VKBase &base, const TextureParams& textureParams, unsigned int width, unsigned int height)
{
	params = textureParams;
	textureType = TextureType::TEXTURE_2D;
	mipLevels = 1;
	this->width = width;
	this->height = height;

	// Make sure the format supports storage
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(base.GetPhysicalDevice(), params.format, &props);

	if (params.usedInCopySrc)
	{
		if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) || !(props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
		{
			std::cout << "Format requested for storage image doesn't support storage\n";
			return false;
		}
	}
	if (params.usedInCopyDst)
	{
		if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) || !(props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
		{
			std::cout << "Format requested for storage image doesn't support storage\n";
			return false;
		}
	}

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = params.format;
	imageCreateInfo.extent.width = static_cast<uint32_t>(width);
	imageCreateInfo.extent.height = static_cast<uint32_t>(height);
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;	
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	if (params.usedInCopySrc)
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (params.usedInCopyDst)
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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

	if (!CreateImageView(device, VK_IMAGE_ASPECT_COLOR_BIT))
		return false;
	if (!CreateSampler(device))
		return false;

	return true;
}

bool VKTexture2D::CreateWithData(VKBase& base, const TextureParams& textureParams, unsigned int width, unsigned int height, const void* data)
{
	params = textureParams;
	textureType = TextureType::TEXTURE_2D;
	mipLevels = 1;
	this->width = width;
	this->height = height;

	// Make sure the format supports storage
	VkFormatProperties props;
	vkGetPhysicalDeviceFormatProperties(base.GetPhysicalDevice(), params.format, &props);

	if (params.useStorage)
	{
		if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
		{
			std::cout << "Format requested for storage image doesn't support storage\n";
			return false;
		}
	}
	if (params.usedInCopySrc)
	{
		if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) || !(props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
		{
			std::cout << "Format requested for storage image doesn't support storage\n";
			return false;
		}
	}
	if (params.usedInCopyDst)
	{
		if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) || !(props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT))
		{
			std::cout << "Format requested for storage image doesn't support storage\n";
			return false;
		}
	}
	
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = params.format;
	imageCreateInfo.extent.width = static_cast<uint32_t>(width);
	imageCreateInfo.extent.height = static_cast<uint32_t>(height);
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

	if (params.useStorage)
		imageCreateInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
	if (params.usedInCopySrc)
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	if (params.usedInCopyDst)
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	/*const vkutils::QueueFamilyIndices queueIndices = base.GetQueueFamilyIndices();

	// If the graphics andcompute queues are different, we create an image that can be shared between them
	// Performance can be worse than exclusive sharing mode, but we save some synchronization to keep this simple
	if (queueIndices.graphicsFamilyIndex != queueIndices.computeFamilyIndex)
	{
		uint32_t indices[] = { queueIndices.graphicsFamilyIndex, queueIndices.computeFamilyIndex };

		imageCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
		imageCreateInfo.queueFamilyIndexCount = 2;
		imageCreateInfo.pQueueFamilyIndices = indices;
	}
	else
	{*/
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	//}

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

	if (params.useStorage)
		base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	else
		base.TransitionImageLayout(image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	if (!CreateImageView(device, VK_IMAGE_ASPECT_COLOR_BIT))
		return false;
	if (!CreateSampler(device))
		return false;

	return true;
}
