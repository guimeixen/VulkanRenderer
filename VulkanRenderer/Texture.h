#pragma once

enum class TextureType {
	TEXTURE_2D,
	TEXTURE_3D,
	TEXTURE_CUBE
};

struct TextureParams {
	VkFormat format;
	VkSamplerAddressMode addressMode;
	VkFilter filter;
	bool useStorage;
	bool dontCreateMipMaps;
	bool usedInCopySrc;
	bool usedInCopyDst;
};
