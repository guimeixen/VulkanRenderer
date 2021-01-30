#pragma once

#include "VKTexture2D.h"

struct FramebufferParams
{
	bool createColorTexture;
	bool createDepthTexture;
	bool sampleDepth;
	std::vector<TextureParams> colorTexturesParams;
	TextureParams depthTexturesParams;
};

class VKFramebuffer
{
public:
	VKFramebuffer();

	bool Create(const VKBase& base, const FramebufferParams &params, unsigned int width, unsigned int height);
	void Dispose(VkDevice device);

	VkFramebuffer GetFramebuffer() const { return framebuffer; }
	VkRenderPass GetRenderPass() const { return renderPass; }
	const VKTexture2D& GetFirstColorTexture() const { return colorTextures[0]; }
	const VKTexture2D& GetDepthTexture() const { return depthTexture; }

	unsigned int GetWidth() const { return width; }
	unsigned int GetHeight() const { return height; }

private:
	unsigned int width;
	unsigned int height;
	VkFramebuffer framebuffer;
	VkRenderPass renderPass;
	std::vector<VKTexture2D> colorTextures;
	VKTexture2D depthTexture;
};

