#include "VolumetricClouds.h"

#include "Random.h"
#include "MeshDefaults.h"
#include "Input.h"

#include <iostream>

VolumetricClouds::VolumetricClouds()
{
	volCloudsData = {};
	cloudUpdateBlockSize = 4;
	cloudsFBWidth = 0;
	cloudsFBHeight = 0;
	frameNumber = 0;
	frameCount = 0;
}

bool VolumetricClouds::Init(VKRenderer* renderer)
{
	cloudsFBWidth = renderer->GetWidth() / 2;
	cloudsFBHeight = renderer->GetHeight() / 2;

	// Make sure the clouds texture is divisible by the block size
	while (cloudsFBWidth % cloudUpdateBlockSize != 0)
	{
		cloudsFBWidth++;
	}
	while (cloudsFBHeight % cloudUpdateBlockSize != 0)
	{
		cloudsFBHeight++;
	}
	unsigned int cloudFBUpdateTextureWidth = cloudsFBWidth / cloudUpdateBlockSize;
	unsigned int cloudFBUpdateTextureHeight = cloudsFBHeight / cloudUpdateBlockSize;

	VKBase& base = renderer->GetBase();

	TextureParams colorParams = {};
	colorParams.format = VK_FORMAT_R8G8B8A8_UNORM;
	colorParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	colorParams.filter = VK_FILTER_LINEAR;

	FramebufferParams params = {};
	params.createColorTexture = true;
	params.createDepthTexture = false;		// We don't need the depth buffer, we're just going to draw one quad
	params.colorTexturesParams.push_back(colorParams);

	cloudsLowResFB.Create(base, params, cloudFBUpdateTextureWidth, cloudFBUpdateTextureHeight);

	//params.colorTexturesParams[0].usedInCopySrc = true;
	//params.colorTexturesParams[0].usedInCopyDst = false;

	cloudsReprojectionFB.Create(base, params, cloudsFBWidth, cloudsFBHeight);
	cloudCopyFB.Create(base, params, cloudsFBWidth, cloudsFBHeight);

	//colorParams.usedInCopySrc = false;
	//colorParams.usedInCopyDst = true;
	//previousFrameTexture[0].CreateWithData(base, colorParams, cloudsFBWidth, cloudsFBHeight, nullptr);
	//previousFrameTexture[1].CreateWithData(base, colorParams, cloudsFBWidth, cloudsFBHeight, nullptr);

	// Create the 3D noise textures
	struct TexData
	{
		unsigned char r;
		unsigned char g;
		unsigned char b;
		unsigned char a;
	};

	const unsigned int resolution = 128;
	TexData* noise = new TexData[resolution * resolution * resolution];

	FILE* file = nullptr;
	file = fopen("Data/Textures/clouds/noise.data", "rb");

	if (!file)
	{
		std::cout << "Failed to open noise file\n";
		return false;
	}

	fread(noise, 1, resolution * resolution * resolution * sizeof(TexData), file);
	fclose(file);

	struct HighFreqNoise
	{
		unsigned char r;
		unsigned char g;
		unsigned char b;
		unsigned char a;	// not used
	};
	const unsigned int highFreqRes = 32;
	HighFreqNoise* highFreqNoise = new HighFreqNoise[highFreqRes * highFreqRes * highFreqRes];

	file = fopen("Data/Textures/clouds/highFreqNoise.data", "rb");

	if (!file)
	{
		std::cout << "Failed to open high freq noise file\n";
		return false;
	}

	fread(highFreqNoise, 1, highFreqRes * highFreqRes * highFreqRes * sizeof(HighFreqNoise), file);
	fclose(file);

	TextureParams noiseParams = {};
	noiseParams.addressMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	noiseParams.format = VK_FORMAT_R8G8B8A8_UNORM;
	noiseParams.filter = VK_FILTER_LINEAR;

	baseNoiseTexture.CreateFromData(base, noiseParams, resolution, resolution, resolution, noise);
	highFreqNoiseTexture.CreateFromData(base, noiseParams, highFreqRes, highFreqRes, highFreqRes, highFreqNoise);

	TextureParams weatherTexParams = {};
	weatherTexParams.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	weatherTexParams.format = VK_FORMAT_R8G8B8A8_UNORM;
	weatherTexParams.filter = VK_FILTER_LINEAR;
	weatherTexParams.dontCreateMipMaps = true;

	weatherTexture.LoadFromFile(base, "Data/Textures/clouds/weather.png", weatherTexParams);

	delete[] noise;
	delete[] highFreqNoise;

	volCloudsData.ambientBottomColor = glm::vec4(0.5f, 0.71f, 1.0f, 0.0f);
	volCloudsData.ambientTopColor = glm::vec4(1.0f);
	volCloudsData.ambientMult = 0.265f;
	volCloudsData.cloudCoverage = 0.45f;
	volCloudsData.cloudStartHeight = 2200.0f;
	volCloudsData.cloudLayerThickness = 2250.0f;
	volCloudsData.cloudLayerTopHeight = volCloudsData.cloudStartHeight + volCloudsData.cloudLayerThickness;
	volCloudsData.densityMult = 2.27f;
	volCloudsData.detailScale = 9.0f;
	volCloudsData.directLightMult = 2.5f;
	volCloudsData.forwardSilverLiningIntensity = 0.35f;
	volCloudsData.hgForward = 0.6f;
	volCloudsData.highCloudsCoverage = 0.0f;
	volCloudsData.highCloudsTimeScale = 0.01f;
	volCloudsData.silverLiningIntensity = 0.65f;
	volCloudsData.silverLiningSpread = 0.88f;
	volCloudsData.timeScale = 0.001f;

	int i = 0;
	for (i = 0; i < 16; i++)
	{
		frameNumbers[i] = i;
	}
	while (i-- > 0)
	{
		int k = frameNumbers[i];
		int j = (int)(Random::Float() * 1000.0f) % 16;
		frameNumbers[i] = frameNumbers[j];
		frameNumbers[j] = k;
	}

	frameNumber = frameNumbers[0];

	MaterialFeatures features = {};
	features.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	features.cullMode = VK_CULL_MODE_BACK_BIT;
	features.enableDepthWrite = VK_TRUE;

	quadMesh = MeshDefaults::CreateQuad(renderer);

	cloudMat.Create(renderer, quadMesh, features, "Data/Shaders/clouds_vert.spv", "Data/Shaders/clouds_frag.spv", cloudsLowResFB.GetRenderPass());

	cloudMatSet = renderer->AllocateUserTextureDescriptorSet();
	renderer->UpdateUserTextureSet3D(cloudMatSet, baseNoiseTexture, 0);
	renderer->UpdateUserTextureSet3D(cloudMatSet, highFreqNoiseTexture, 1);
	renderer->UpdateUserTextureSet2D(cloudMatSet, weatherTexture, 2);

	cloudReprojectionMat.Create(renderer, quadMesh, features, "Data/Shaders/cloud_reprojection_vert.spv", "Data/Shaders/cloud_reprojection_frag.spv", cloudsReprojectionFB.GetRenderPass());
	cloudCopyMat.Create(renderer, quadMesh, features, "Data/Shaders/cloud_reprojection_vert.spv", "Data/Shaders/cloud_copy_frag.spv", cloudCopyFB.GetRenderPass());


	cloudReprojectionSet[0] = renderer->AllocateUserTextureDescriptorSet();
	//cloudReprojectionSet[1] = renderer->AllocateUserTextureDescriptorSet();

	renderer->UpdateUserTextureSet2D(cloudReprojectionSet[0], cloudsLowResFB.GetFirstColorTexture(), 0);
	renderer->UpdateUserTextureSet2D(cloudReprojectionSet[0], cloudCopyFB.GetFirstColorTexture(), 1);

	//renderer->UpdateUserTextureSet2D(cloudReprojectionSet[1], cloudsLowResFB.GetFirstColorTexture(), 0);
	//renderer->UpdateUserTextureSet2D(cloudReprojectionSet[1], previousFrameTexture[1], 1);

	return true;
}

void VolumetricClouds::Dispose(VkDevice device)
{
	cloudsLowResFB.Dispose(device);
	cloudsReprojectionFB.Dispose(device);
	cloudCopyFB.Dispose(device);
//	previousFrameTexture[0].Dispose(device);
	//previousFrameTexture[1].Dispose(device);
	baseNoiseTexture.Dispose(device);
	highFreqNoiseTexture.Dispose(device);
	weatherTexture.Dispose(device);
	cloudMat.Dispose(device);
	cloudReprojectionMat.Dispose(device);
	cloudCopyMat.Dispose(device);
	quadMesh.vb.Dispose(device);
}

void VolumetricClouds::PerformCloudsPass(VKRenderer *renderer, VkCommandBuffer cmdBuffer)
{
	VkViewport viewport = {};
	viewport.maxDepth = 1.0f;
	viewport.width = (float)cloudsLowResFB.GetWidth();
	viewport.height = (float)cloudsLowResFB.GetHeight();

	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

	VkClearValue clearValue = {};
	clearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f };

	VkBuffer vertexBuffers[] = { VK_NULL_HANDLE };
	VkDeviceSize offsets[] = { 0 };

	vertexBuffers[0] = quadMesh.vb.GetBuffer();

	renderer->BeginRenderPass(cmdBuffer, cloudsLowResFB, 1, &clearValue);	
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cloudMat.GetPipeline());
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->GetPipelineLayout(), 2, 1, &cloudMatSet, 0, nullptr);
	vkCmdDraw(cmdBuffer, 6, 1, 0, 0);
	vkCmdEndRenderPass(cmdBuffer);

	// Wait for the copy to the previous frame texture to be complete
	VkMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
	/*	VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;
		barrier.image = previousFrameTexture[(currentFrame + 1) % 2].GetImage();*/

	/*barrier.srcAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
		VK_ACCESS_INDEX_READ_BIT |
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
		VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_TRANSFER_READ_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_HOST_READ_BIT |
		VK_ACCESS_HOST_WRITE_BIT,
		barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT |
		VK_ACCESS_INDEX_READ_BIT |
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
		VK_ACCESS_UNIFORM_READ_BIT |
		VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
		VK_ACCESS_SHADER_READ_BIT |
		VK_ACCESS_SHADER_WRITE_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
		VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
		VK_ACCESS_TRANSFER_READ_BIT |
		VK_ACCESS_TRANSFER_WRITE_BIT |
		VK_ACCESS_HOST_READ_BIT |
		VK_ACCESS_HOST_WRITE_BIT;
	*/

	//vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);

	viewport.width = (float)cloudsReprojectionFB.GetWidth();
	viewport.height = (float)cloudsReprojectionFB.GetHeight();

	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

	renderer->BeginRenderPass(cmdBuffer, cloudsReprojectionFB, 1, &clearValue);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cloudReprojectionMat.GetPipeline());
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->GetPipelineLayout(), 2, 1, &cloudReprojectionSet[0], 0, nullptr);
	vkCmdDraw(cmdBuffer, 6, 1, 0, 0);
	vkCmdEndRenderPass(cmdBuffer);

	renderer->BeginRenderPass(cmdBuffer, cloudCopyFB, 1, &clearValue);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cloudCopyMat.GetPipeline());
	vkCmdDraw(cmdBuffer, 6, 1, 0, 0);
	vkCmdEndRenderPass(cmdBuffer);

	// Copy the current frame cloud texture to the previous frame texture so we can use it in the next frame
	/*VkOffset3D blitSize;
	blitSize.x = cloudsReprojectionFB.GetWidth();
	blitSize.y = cloudsReprojectionFB.GetHeight();
	blitSize.z = 1;

	VkImageBlit blit = {};
	blit.srcOffsets[1] = blitSize;
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.layerCount = 1;

	blit.dstOffsets[1] = blitSize;
	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.layerCount = 1;

	VKBase& base = renderer->GetBase();

	VkImage srcImg = cloudsReprojectionFB.GetFirstColorTexture().GetImage();
	VkImage dstImg = previousFrameTexture[renderer->GetCurrentFrame()].GetImage();

	VkImageCopy region = {};
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.layerCount = 1;
	region.extent.width = cloudsReprojectionFB.GetWidth();
	region.extent.height = cloudsReprojectionFB.GetHeight();
	region.extent.depth = 1;

	if (!Input::IsKeyPressed(KEY_SPACE))
	{
		VkImageMemoryBarrier b[2] = {};
		b[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b[0].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		b[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		b[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b[0].subresourceRange.layerCount = 1;
		b[0].subresourceRange.levelCount = 1;
		b[0].image = srcImg;

		b[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		b[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b[1].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		b[1].subresourceRange.layerCount = 1;
		b[1].subresourceRange.levelCount = 1;
		b[1].image = dstImg;

		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, b);

		//vkCmdBlitImage(cmdBuffer, srcImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
		vkCmdCopyImage(cmdBuffer, srcImg, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		b[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		b[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		b[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b[0].image = srcImg;

		b[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		b[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		b[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		b[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		b[1].image = dstImg;

		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, b);
	}*/
}

void VolumetricClouds::EndFrame()
{
	frameCount++;
	if (frameCount >= 16)
		frameCount = 0;
	frameNumber = frameNumbers[frameCount];
	//frameNumber = (frameNumber + 1) % (cloudUpdateBlockSize * cloudUpdateBlockSize);
}

glm::mat4 VolumetricClouds::GetJitterMatrix() const
{
	// Create the jitter matrix that will be use to translate the projection matrix
	int x = frameNumber % cloudUpdateBlockSize;
	int y = frameNumber / cloudUpdateBlockSize;

	float offsetX = x * 2.0f / cloudsFBWidth;		// The size of a pixel is 1/width but because the range we're applying the offset is in [-1,1] we have to multiply by 2 to get the size of a pixel
	float offsetY = y * 2.0f / cloudsFBHeight;

	glm::mat4 jitterMatrix = glm::mat4(1.0f);
	jitterMatrix[3] = glm::vec4(offsetX, offsetY, 0.0f, 1.0f);

	return jitterMatrix;
}
