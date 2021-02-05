#pragma once

#include "VKBase.h"
#include "Texture.h"

class VKTexture3D
{
public:
	VKTexture3D();

	bool CreateFromData(VKBase &base, const TextureParams& params, unsigned int width, unsigned int height, unsigned int depth, const void* data);
	void Dispose(VkDevice device);

	VkImage GetImage() const { return image; }
	VkImageView GetImageView() const { return imageView; }
	VkSampler GetSampler() const { return sampler; }
	VkFormat GetFormat() const { return params.format; }
	unsigned int GetNumMipLevels() const { return mipLevels; }
	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }

private:
	bool CreateImageView(VkDevice device, VkImageAspectFlags imageAspect);
	bool CreateSampler(VkDevice device);

private:
	VkImage image;
	VkImageView imageView;
	VkSampler sampler;
	VkDeviceMemory memory;
	TextureParams params;
	unsigned int width;
	unsigned int height;
	unsigned int depth;
	unsigned int mipLevels;
};

