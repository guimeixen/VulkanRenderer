#pragma once

#include "VKBase.h"
#include "Texture.h"

#include <string>

class VKTexture2D
{
public:
	VKTexture2D();

	bool LoadFromFile(VKBase& base, const std::string& path, const TextureParams &textureParams);
	bool LoadCubemapFromFiles(VKBase& base, const std::vector<std::string>& facesPath, const TextureParams& textureParams);
	bool CreateDepthTexture(const VKBase &base, const TextureParams& textureParams, unsigned int width, unsigned int height, bool sampled);
	// Right now the function assumes the color texture will be sampled
	bool CreateColorTexture(const VKBase& base, const TextureParams& textureParams, unsigned int width, unsigned int height);
	bool CreateWithData(VKBase& base, const TextureParams& textureParams, unsigned int width, unsigned int height, const void* data);
	void Dispose(VkDevice device);

	VkImage GetImage() const { return image; }
	VkImageView GetImageView() const { return imageView; }
	VkSampler GetSampler() const { return sampler; }
	VkFormat GetFormat() const { return params.format; }
	unsigned int GetNumMipLevels() const { return mipLevels; }
	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }

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
	unsigned int mipLevels;
	TextureType textureType;
};
