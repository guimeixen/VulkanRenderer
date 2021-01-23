#pragma once

#include "VKBase.h"

#include <string>

enum class TextureType {
	TEXTURE_2D,
	TEXTURE_3D,
	TEXTURE_CUBE
};

struct TextureParams {
	VkFormat format;
	VkSamplerAddressMode addressMode;
	VkFilter filter;
};

class VKTexture2D
{
public:
	VKTexture2D();

	bool LoadFromFile(const std::string &path, VKBase &base, const TextureParams &textureParams);
	bool LoadCubemapFromFiles(const std::vector<std::string>& facesPath, VKBase& base, const TextureParams& textureParams);
	bool CreateDepthTexture(const VKBase &base, const TextureParams& textureParams, unsigned int width, unsigned int height, bool sampled);
	// Right now the function assumes the color texture will be sampled
	bool CreateColorTexture(const VKBase& base, const TextureParams& textureParams, unsigned int width, unsigned int height);
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
	TextureType textureType;
};
