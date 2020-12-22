#pragma once

#include "VKBase.h"

#include <string>

struct TextureParams {
	VkFormat format;
};

class VKTexture2D
{
public:
	VKTexture2D();

	bool LoadFromFile(const std::string &path, VKBase &base, const TextureParams &textureParams);
	bool CreateDepthTexture(VKBase &base, const TextureParams& textureParams, unsigned int width, unsigned int height);
	void Dispose(VkDevice device);

	VkImage GetImage() const { return image; }
	VkImageView GetImageView() const { return imageView; }
	VkSampler GetSampler() const { return sampler; }
	VkFormat GetFormat() const { return params.format; }

private:
	bool CreateImage(VkDevice device);
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
};
