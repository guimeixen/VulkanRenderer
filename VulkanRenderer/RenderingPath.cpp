#include "RenderingPath.h"

#include "VKRenderer.h"
#include "VKTexture2D.h"
#include "VKFramebuffer.h"
#include "VertexTypes.h"
#include "MeshDefaults.h"

#include <iostream>

RenderingPath::RenderingPath()
{
	width = 0;
	height = 0;
	renderer = nullptr;
	postQuadSet = VK_NULL_HANDLE;
	computeFence = VK_NULL_HANDLE;
	computeSemaphore = VK_NULL_HANDLE;
	computeSet = VK_NULL_HANDLE;
	graphicsSemaphore = VK_NULL_HANDLE;

	previousFrameView = glm::mat4(1.0f);
}

bool RenderingPath::Init(VKRenderer* renderer, unsigned int width, unsigned int height)
{
	this->renderer = renderer;
	this->width = width;
	this->height = height;

	VKBase& base = renderer->GetBase();
	VkDevice device = base.GetDevice();

	if (!CreateShadowMapPass())
		return false;
	if (!CreateHDRPass())
		return false;

	if (!projectedGridWater.Load(renderer, hdrFB.GetRenderPass()))
		return false;

	if (!volClouds.Init(renderer))
		return false;
	
	std::vector<std::string> faces(6);
	faces[0] = "Data/Textures/left.png";
	faces[1] = "Data/Textures/right.png";
	faces[2] = "Data/Textures/up.png";
	faces[3] = "Data/Textures/down.png";
	faces[4] = "Data/Textures/front.png";
	faces[5] = "Data/Textures/back.png";

	if (!skybox.Load(renderer, faces, hdrFB.GetRenderPass()))
	{
		std::cout << "Failed to load skybox\n";
		return false;
	}

	TextureParams storageTexParams = {};
	storageTexParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	storageTexParams.filter = VK_FILTER_LINEAR;
	storageTexParams.format = VK_FORMAT_R8G8B8A8_UNORM;
	storageTexParams.useStorage = true;

	storageTexture.CreateWithData(base, storageTexParams, 256, 256, nullptr);

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	imageInfo.imageView = shadowFB.GetDepthTexture().GetImageView();
	imageInfo.sampler = shadowFB.GetDepthTexture().GetSampler();

	VkDescriptorImageInfo imageInfo2 = {};
	imageInfo2.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imageInfo2.imageView = storageTexture.GetImageView();
	imageInfo2.sampler = storageTexture.GetSampler();

	VkDescriptorImageInfo imageInfo3 = {};
	imageInfo3.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo3.imageView = volClouds.GetCloudsTexture().GetImageView();
	imageInfo3.sampler = volClouds.GetCloudsTexture().GetSampler();

	renderer->UpdateGlobalTexturesSet(imageInfo, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	renderer->UpdateGlobalTexturesSet(imageInfo2, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	renderer->UpdateGlobalTexturesSet(imageInfo3, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	
	dirLightUBO.Create(&base, sizeof(DirLightUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Create a large buffer which will hold the model matrices
	instanceDataBuffer.Create(&base, sizeof(glm::mat4) * 32, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = instanceDataBuffer.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	renderer->UpdateGlobalBuffersSet(bufferInfo, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	bufferInfo.buffer = dirLightUBO.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	renderer->UpdateGlobalBuffersSet(bufferInfo, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	if (!CreatePostProcessPass())
		return false;
	if (!CreateComputePass())
		return false;
	
	lightSpaceCamera.SetProjectionMatrix(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 10.0f);
	lightSpaceCamera.SetViewMatrix(glm::vec3(0.0f, 2.0f, 2.5f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	return true;
}

void RenderingPath::Update(const Camera& camera, float deltaTime)
{
	projectedGridWater.Update(camera, deltaTime);		// Make sure to update the grid before updating the frame data buffer otherwise the shader will get old values and will cause problems at the edge of the image when rotating the camera
}

void RenderingPath::EndFrame(const Camera &camera)
{
	volClouds.EndFrame();
	previousFrameView = camera.GetViewMatrix();
}

void RenderingPath::Dispose()
{
	VkDevice device = renderer->GetBase().GetDevice();

	quadMat.Dispose(device);
	quadMesh.vb.Dispose(device);
	computeMat.Dispose(device);
	volClouds.Dispose(device);
	projectedGridWater.Dispose(device);
	hdrFB.Dispose(device);
	shadowFB.Dispose(device);
	instanceDataBuffer.Dispose(device);
	dirLightUBO.Dispose(device);

	vkDestroyFence(device, computeFence, nullptr);
	storageTexture.Dispose(device);
	vkDestroySemaphore(device, computeSemaphore, nullptr);
	vkDestroySemaphore(device, graphicsSemaphore, nullptr);

	skybox.Dispose(device);
	postQuadMat.Dispose(device);
	shadowMat.Dispose(device);
}

bool RenderingPath::CreateShadowMapPass()
{
	VKBase& base = renderer->GetBase();

	// Shadow mapping framebuffer
	FramebufferParams shadowFBParams = {};
	shadowFBParams.createColorTexture = false;
	shadowFBParams.createDepthTexture = true;
	shadowFBParams.sampleDepth = true;
	shadowFBParams.depthTexturesParams.format = VK_FORMAT_D16_UNORM;
	shadowFBParams.depthTexturesParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	VkFilter filter = vkutils::IsFormatFilterable(base.GetPhysicalDevice(), VK_FORMAT_D16_UNORM, VK_IMAGE_TILING_OPTIMAL) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

	shadowFBParams.depthTexturesParams.filter = filter;

	if (!shadowFB.Create(base, shadowFBParams, 1024, 1024))
		return false;


	VkVertexInputBindingDescription bindingDesc = {};
	bindingDesc = {};
	bindingDesc.binding = 0;
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc.stride = sizeof(Vertex);

	VkVertexInputAttributeDescription attribDesc[3] = {};
	attribDesc[0].binding = 0;
	attribDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDesc[0].location = 0;
	attribDesc[0].offset = 0;

	attribDesc[1].binding = 0;
	attribDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
	attribDesc[1].location = 1;
	attribDesc[1].offset = offsetof(Vertex, uv);

	attribDesc[2].binding = 0;
	attribDesc[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	attribDesc[2].location = 2;
	attribDesc[2].offset = offsetof(Vertex, normal);

	shadowMesh.bindings.push_back(bindingDesc);
	shadowMesh.attribs.push_back(attribDesc[0]);
	shadowMesh.attribs.push_back(attribDesc[1]);
	shadowMesh.attribs.push_back(attribDesc[2]);

	MaterialFeatures shadowMatFeatures = {};
	shadowMatFeatures.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	shadowMatFeatures.cullMode = VK_CULL_MODE_BACK_BIT;
	shadowMatFeatures.enableDepthWrite = VK_TRUE;
	
	if (!shadowMat.Create(renderer, shadowMesh, shadowMatFeatures, "shadow", "shadow", shadowFB.GetRenderPass()))
		return false;

	return true;
}

bool RenderingPath::CreateHDRPass()
{
	VKBase& base = renderer->GetBase();

	// Offscreen framebuffer
	TextureParams colorParams = {};
	colorParams.format = VK_FORMAT_R8G8B8A8_UNORM;
	colorParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	colorParams.filter = VK_FILTER_LINEAR;
	TextureParams depthParams = {};
	depthParams.format = vkutils::FindSupportedDepthFormat(base.GetPhysicalDevice());

	FramebufferParams fbParams = {};
	fbParams.createColorTexture = true;
	fbParams.createDepthTexture = true;
	fbParams.sampleDepth = false;
	fbParams.colorTexturesParams.push_back(colorParams);
	fbParams.depthTexturesParams = depthParams;

	if (!hdrFB.Create(base, fbParams, width, height))
		return false;

	return true;
}

bool RenderingPath::CreatePostProcessPass()
{
	MaterialFeatures postQuadMatFeatures = {};
	postQuadMatFeatures.cullMode = VK_CULL_MODE_FRONT_BIT;
	postQuadMatFeatures.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	postQuadMatFeatures.enableDepthWrite = VK_TRUE;

	// Empty mesh to render the quad with no buffers
	postQuadMesh = {};
	postQuadMesh.vertexCount = 3;
	if (!postQuadMat.Create(renderer, postQuadMesh, postQuadMatFeatures, "post_quad", "post_quad", renderer->GetDefaultRenderPass()))
		return false;

	postQuadSet = renderer->AllocateUserTextureDescriptorSet();
	renderer->UpdateUserTextureSet2D(postQuadSet, hdrFB.GetFirstColorTexture(), 0);

	return true;
}

bool RenderingPath::CreateComputePass()
{
	VKBase& base = renderer->GetBase();
	VkDevice device = base.GetDevice();

	computeMat.Create(renderer, "compute");
	computeSet = renderer->AllocateSetFromLayout(computeMat.GetSetLayout());

	VkDescriptorImageInfo compImgInfo = {};
	compImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	compImgInfo.imageView = storageTexture.GetImageView();
	compImgInfo.sampler = storageTexture.GetSampler();

	VkWriteDescriptorSet computeWrite = {};
	computeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	computeWrite.descriptorCount = 1;
	computeWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	computeWrite.dstArrayElement = 0;
	computeWrite.dstBinding = 0;
	computeWrite.dstSet = computeSet;
	computeWrite.pImageInfo = &compImgInfo;

	vkUpdateDescriptorSets(device, 1, &computeWrite, 0, nullptr);

	// Semaphore for compute and graphics sync
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeSemaphore) != VK_SUCCESS)
	{
		std::cout << "Failed to create compute semaphore\n";
		return false;
	}
	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &graphicsSemaphore) != VK_SUCCESS)
	{
		std::cout << "Failed to create graphics semaphore\n";
		return false;
	}


	// Signal the semaphore
	VkSubmitInfo semaInfo = {};
	semaInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	semaInfo.signalSemaphoreCount = 1;
	semaInfo.pSignalSemaphores = &graphicsSemaphore;

	if (vkQueueSubmit(base.GetGraphicsQueue(), 1, &semaInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		std::cout << "Failed to submit\n";
		return false;
	}
	vkQueueWaitIdle(base.GetGraphicsQueue());

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	if (vkCreateFence(device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS)
	{
		std::cout << "Failed to create fence\n";
		return false;
	}

	// Create quad to display the image create by the compute shader
	MaterialFeatures quadMatFeatures = {};
	quadMatFeatures.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	quadMatFeatures.cullMode = VK_CULL_MODE_BACK_BIT;
	quadMatFeatures.enableDepthWrite = VK_TRUE;

	quadMesh = MeshDefaults::CreateQuad(renderer);
	quadMat.Create(renderer, quadMesh, quadMatFeatures, "quad", "quad", hdrFB.GetRenderPass());

	return true;
}

bool RenderingPath::PerformComputePass()
{
	VKBase& base = renderer->GetBase();
	VkDevice device = base.GetDevice();

	const vkutils::QueueFamilyIndices& indices = base.GetQueueFamilyIndices();

	// Record the compute command buffer
	computeCmdBuffer = renderer->CreateComputeCommandBuffer(true);

	VkImageSubresourceRange range = {};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseArrayLayer = 0;
	range.baseMipLevel = 0;
	range.layerCount = 1;
	range.levelCount = 1;

	if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
	{
		// Acquire
		VkImageMemoryBarrier acquireBarrier = {};
		acquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		acquireBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		acquireBarrier.srcAccessMask = 0;
		acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		acquireBarrier.srcQueueFamilyIndex = indices.graphicsFamilyIndex;
		acquireBarrier.dstQueueFamilyIndex = indices.computeFamilyIndex;
		acquireBarrier.subresourceRange = range;
		acquireBarrier.image = storageTexture.GetImage();

		vkCmdPipelineBarrier(computeCmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &acquireBarrier);
	}

	vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeMat.GetPipeline());
	vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeMat.GetPipelineLayout(), 0, 1, &computeSet, 0, nullptr);
	vkCmdDispatch(computeCmdBuffer, 256 / 16, 256 / 16, 1);

	if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
	{
		// Release
		VkImageMemoryBarrier releaseBarrier = {};
		releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		releaseBarrier.dstAccessMask = 0;
		releaseBarrier.srcQueueFamilyIndex = indices.computeFamilyIndex;
		releaseBarrier.dstQueueFamilyIndex = indices.graphicsFamilyIndex;
		releaseBarrier.subresourceRange = range;
		releaseBarrier.image = storageTexture.GetImage();

		vkCmdPipelineBarrier(computeCmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &releaseBarrier);
	}

	vkEndCommandBuffer(computeCmdBuffer);

	// Transfer ownership
	// If graphics and compute queue family indices differ, acquire and immediately release the storage image,
	// so that the initial acquire from the graphics command buffers are matched up properly

	if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
	{
		// Release ownership from graphics queue to compute queue
		VkCommandBuffer cmd = renderer->CreateGraphicsCommandBuffer(true);

		VkImageMemoryBarrier releaseBarrier = {};
		releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.srcAccessMask = 0;
		releaseBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		releaseBarrier.srcQueueFamilyIndex = indices.graphicsFamilyIndex;
		releaseBarrier.dstQueueFamilyIndex = indices.computeFamilyIndex;
		releaseBarrier.subresourceRange = range;
		releaseBarrier.image = storageTexture.GetImage();

		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &releaseBarrier);

		vkEndCommandBuffer(cmd);

		VkSubmitInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		info.commandBufferCount = 1;
		info.pCommandBuffers = &cmd;

		if (vkQueueSubmit(base.GetGraphicsQueue(), 1, &info, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			std::cout << "Failed to submit\n";
			return false;
		}
		vkQueueWaitIdle(base.GetGraphicsQueue());		// Or wait for fence?

		renderer->FreeGraphicsCommandBuffer(cmd);
	}

	return true;
}

bool RenderingPath::SubmitCompute()
{
	VKBase& base = renderer->GetBase();
	VkDevice device = base.GetDevice();

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

	VkSubmitInfo computeSubmitInfo = {};
	computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	computeSubmitInfo.waitSemaphoreCount = 1;
	computeSubmitInfo.pWaitSemaphores = &graphicsSemaphore;
	computeSubmitInfo.pWaitDstStageMask = &waitStage;
	computeSubmitInfo.commandBufferCount = 1;
	computeSubmitInfo.pCommandBuffers = &computeCmdBuffer;
	computeSubmitInfo.signalSemaphoreCount = 1;
	computeSubmitInfo.pSignalSemaphores = &computeSemaphore;

	vkWaitForFences(device, 1, &computeFence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &computeFence);

	if (vkQueueSubmit(base.GetComputeQueue(), 1, &computeSubmitInfo, computeFence) != VK_SUCCESS)
	{
		std::cout << "Failed to submit\n";
		return false;
	}

	return true;
}

bool RenderingPath::PerformShadowMapPass(VkCommandBuffer cmdBuffer, ModelManager& modelManager)
{
	VkPipelineLayout pipelineLayout = renderer->GetPipelineLayout();

	VkViewport viewport = {};
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	viewport.width = (float)shadowFB.GetWidth();
	viewport.height = (float)shadowFB.GetHeight();

	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

	renderer->SetCamera(lightSpaceCamera);

	VkClearValue depthClearValue = {};
	depthClearValue.depthStencil = { 1.0f, 0 };

	renderer->BeginRenderPass(cmdBuffer, shadowFB, 1, &depthClearValue);
	modelManager.Render(cmdBuffer, pipelineLayout, shadowMat.GetPipeline());
	vkCmdEndRenderPass(cmdBuffer);

	return true;
}

bool RenderingPath::PerformVolumetricCloudsPass(VkCommandBuffer cmdBuffer)
{
	volClouds.PerformCloudsPass(renderer, cmdBuffer);

	return true;
}

bool RenderingPath::PerformHDRPass(VkCommandBuffer cmdBuffer, ModelManager& modelManager, ParticleManager& particleManager)
{
	VkPipelineLayout pipelineLayout = renderer->GetPipelineLayout();

	VKBase& base = renderer->GetBase();
	VkExtent2D surfaceExtent = base.GetSurfaceExtent();

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	viewport.width = (float)surfaceExtent.width;
	viewport.height = (float)surfaceExtent.height;

	vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

	const vkutils::QueueFamilyIndices& indices = base.GetQueueFamilyIndices();

	VkImageSubresourceRange range = {};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseArrayLayer = 0;
	range.baseMipLevel = 0;
	range.layerCount = 1;
	range.levelCount = 1;

	if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
	{
		// Acquire
		VkImageMemoryBarrier acquireBarrier = {};
		acquireBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		acquireBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		acquireBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		acquireBarrier.srcAccessMask = 0;
		acquireBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		acquireBarrier.srcQueueFamilyIndex = indices.computeFamilyIndex;
		acquireBarrier.dstQueueFamilyIndex = indices.graphicsFamilyIndex;
		acquireBarrier.subresourceRange = range;
		acquireBarrier.image = storageTexture.GetImage();

		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &acquireBarrier);

		//renderer.AcquireImageBarrier(cmdBuffer, storageTexture, indices.computeFamilyIndex, indices.graphicsFamilyIndex);
	}


	VkClearValue clearValues[2] = {};
	clearValues[0].color = { 0.3f, 0.3f, 0.3f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderer->BeginRenderPass(cmdBuffer, hdrFB, 2, clearValues);

	modelManager.Render(cmdBuffer, pipelineLayout, VK_NULL_HANDLE);

	VkBuffer vertexBuffers[] = { VK_NULL_HANDLE };
	VkDeviceSize offsets[] = { 0 };

	vertexBuffers[0] = quadMesh.vb.GetBuffer();
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadMat.GetPipeline());
	vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

	projectedGridWater.Render(cmdBuffer, pipelineLayout);

	// Render skybox as last
	skybox.Render(cmdBuffer, pipelineLayout);

	// Particle systems have to be rendered after the skybox
	particleManager.Render(cmdBuffer, renderer->GetPipelineLayout());


	vkCmdEndRenderPass(cmdBuffer);

	if (indices.graphicsFamilyIndex != indices.computeFamilyIndex)
	{
		// Release
		VkImageMemoryBarrier releaseBarrier = {};
		releaseBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		releaseBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		releaseBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
		releaseBarrier.dstAccessMask = 0;
		releaseBarrier.srcQueueFamilyIndex = indices.graphicsFamilyIndex;
		releaseBarrier.dstQueueFamilyIndex = indices.computeFamilyIndex;
		releaseBarrier.subresourceRange = range;
		releaseBarrier.image = storageTexture.GetImage();

		vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &releaseBarrier);

		//renderer.ReleaseImageBarrier(cmdBuffer, storageTexture, indices.graphicsFamilyIndex, indices.computeFamilyIndex);
	}

	return true;
}

bool RenderingPath::PerformPostProcessPass(VkCommandBuffer cmdBuffer)
{
	VkPipelineLayout pipelineLayout = renderer->GetPipelineLayout();

	renderer->BeginDefaultRenderPass();

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postQuadMat.GetPipeline());
	vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, USER_TEXTURES_SET_BINDING, 1, &postQuadSet, 0, nullptr);
	vkCmdDraw(cmdBuffer, (uint32_t)postQuadMesh.vertexCount, 1, 0, 0);

	renderer->EndDefaultRenderPass();

	return true;
}

void RenderingPath::UpdateBuffers(const Camera &camera, const ModelManager& modelManager, TransformManager &transformManager, float deltaTime, float timeElapsed)
{
	VkDevice device = renderer->GetBase().GetDevice();

	const VolumetricCloudsData& volCloudsData = volClouds.GetVolumetricCloudsData();

	frameData.screenRes = glm::vec2((float)width, (float)height);
	frameData.deltaTime = deltaTime;
	frameData.timeElapsed = timeElapsed;
	frameData.ambientBottomColor = volCloudsData.ambientBottomColor;
	frameData.ambientMult = volCloudsData.ambientMult;
	frameData.ambientTopColor = volCloudsData.ambientTopColor;
	frameData.cloudCoverage = volCloudsData.cloudCoverage;
	frameData.cloudStartHeight = volCloudsData.cloudStartHeight;
	frameData.cloudLayerThickness = volCloudsData.cloudLayerThickness;
	frameData.cloudLayerTopHeight = volCloudsData.cloudLayerTopHeight;
	frameData.timeScale = 0.0f;
	frameData.hgForward = volCloudsData.hgForward;
	frameData.densityMult = volCloudsData.densityMult;
	frameData.detailScale = volCloudsData.detailScale;
	frameData.highCloudsCoverage = volCloudsData.highCloudsCoverage;
	frameData.highCloudsTimeScale = volCloudsData.highCloudsTimeScale;
	frameData.silverLiningIntensity = volCloudsData.silverLiningIntensity;
	frameData.silverLiningSpread = volCloudsData.cloudCoverage;
	frameData.forwardSilverLiningIntensity = volCloudsData.forwardSilverLiningIntensity;
	frameData.directLightMult = volCloudsData.directLightMult;
	frameData.cloudsInvProjJitter = glm::inverse(camera.GetProjectionMatrix()) * volClouds.GetJitterMatrix();
	frameData.frameNumber = volClouds.GetFrameNumber();
	frameData.previousFrameView = previousFrameView;
	frameData.cloudUpdateBlockSize = volClouds.GetUpdateBlockSize();
	frameData.projGridViewFrame = projectedGridWater.GetViewFrame();
	frameData.viewCorner0 = projectedGridWater.GetViewCorner0();
	frameData.viewCorner1 = projectedGridWater.GetViewCorner1();
	frameData.viewCorner2 = projectedGridWater.GetViewCorner2();
	frameData.viewCorner3 = projectedGridWater.GetViewCorner3();
	//frameData.waterNormalMapOffset = projectedGridWater.GetNormalMapOffset();

	renderer->UpdateFrameUBO(frameData);

	DirLightUBO dirLightData = {};
	dirLightData.lightSpaceMatrix[0] = lightSpaceCamera.GetProjectionMatrix() * lightSpaceCamera.GetViewMatrix();

	void* mapped = dirLightUBO.Map(device, 0, VK_WHOLE_SIZE);
	memcpy(mapped, &dirLightData, sizeof(DirLightUBO));
	dirLightUBO.Unmap(device);

	// Update model matrices
	const std::vector<ModelInstance>& modelInstances = modelManager.GetModelInstances();

	mapped = instanceDataBuffer.Map(device, 0, VK_WHOLE_SIZE);
	size_t offset = 0;
	for (unsigned int i = 0; i < modelManager.GetNumModels(); i++)
	{
		void* ptr = (char*)mapped + offset;
		memcpy(ptr, &transformManager.GetLocalToWorld(modelInstances[i].e), sizeof(glm::mat4));
		offset += sizeof(glm::mat4);
	}

	instanceDataBuffer.Unmap(device);
}