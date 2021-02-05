#include "Window.h"
#include "Input.h"
#include "Random.h"
#include "Camera.h"
#include "VertexTypes.h"
#include "ParticleManager.h"
#include "ModelManager.h"
#include "Skybox.h"
#include "UniformBufferTypes.h"
#include "EntityManager.h"
#include "TransformManager.h"
#include "Allocator.h"
#include "MeshDefaults.h"
#include "Material.h"
#include "ComputeMaterial.h"
#include "VolumetricClouds.h"

#include "glm/gtc/matrix_transform.hpp"

#include <iostream>
#include <set>
#include <array>
#include <chrono>

unsigned int width = 640;
unsigned int height = 480;

int main()
{
	Random::Init();
	InputManager inputManager;
	Window window;
	window.Init(&inputManager, width, height);

	Allocator allocator;
	EntityManager entityManager;
	TransformManager transformManager;
	transformManager.Init(&allocator, 10);

	VKRenderer* renderer = new VKRenderer();
	if (!renderer->Init(window.GetHandle(), width, height))
	{
		glfwTerminate();
		return 1;
	}

	VKBase& base = renderer->GetBase();

	VkDevice device = base.GetDevice();
	VkExtent2D surfaceExtent = base.GetSurfaceExtent();
	VkSurfaceFormatKHR surfaceFormat = base.GetSurfaceFormat();

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

	VKFramebuffer offscreenFB;
	offscreenFB.Create(base, fbParams, width, height);

	// Shadow mapping framebuffer
	FramebufferParams shadowFBParams = {};
	shadowFBParams.createColorTexture = false;
	shadowFBParams.createDepthTexture = true;
	shadowFBParams.sampleDepth = true;
	shadowFBParams.depthTexturesParams.format = VK_FORMAT_D16_UNORM;
	shadowFBParams.depthTexturesParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	VkFilter filter = vkutils::IsFormatFilterable(base.GetPhysicalDevice(), VK_FORMAT_D16_UNORM, VK_IMAGE_TILING_OPTIMAL) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;

	shadowFBParams.depthTexturesParams.filter = filter;

	VKFramebuffer shadowFB;
	shadowFB.Create(base, shadowFBParams, 1024, 1024);

	VolumetricClouds volClouds;
	volClouds.Init(renderer);


	ModelManager modelManager;
	if (!modelManager.Init(renderer, offscreenFB.GetRenderPass()))
	{
		std::cout << "Failed to init model manager\n";
		return 1;
	}
	
	Entity trashCanEntity = entityManager.Create();
	Entity floorEntity = entityManager.Create();
	transformManager.AddTransform(trashCanEntity);
	transformManager.AddTransform(floorEntity);

	transformManager.SetLocalPosition(trashCanEntity, glm::vec3(0.0f, 0.5f, 0.0f));

	modelManager.AddModel(renderer, trashCanEntity, "Data/Models/trash_can.obj", "Data/Models/trash_can_d.jpg");
	modelManager.AddModel(renderer, floorEntity, "Data/Models/floor.obj", "Data/Models/floor.jpg");

	std::vector<std::string> faces(6);
	faces[0] = "Data/Textures/left.png";
	faces[1] = "Data/Textures/right.png";
	faces[2] = "Data/Textures/up.png";
	faces[3] = "Data/Textures/down.png";
	faces[4] = "Data/Textures/front.png";
	faces[5] = "Data/Textures/back.png";

	Skybox skybox;
	if (!skybox.Load(renderer, faces, offscreenFB.GetRenderPass()))
	{
		std::cout << "Failed to load skybox\n";
		return 1;
	}
	
	ParticleManager particleManager;
	if (!particleManager.Init(renderer, offscreenFB.GetRenderPass()))
		return 1;

	if (!particleManager.AddParticleSystem(renderer, "Data/Textures/particleTexture.png", 10))
	{
		std::cout << "Failed to add particle system\n";
		return 1;
	}

	TextureParams storageTexParams = {};
	storageTexParams.addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	storageTexParams.filter = VK_FILTER_LINEAR;
	storageTexParams.format = VK_FORMAT_R8G8B8A8_UNORM;
	storageTexParams.useStorage = true;

	VKTexture2D storageTexture;
	storageTexture.CreateWithData(base, storageTexParams, 256, 256, nullptr);

	VKBuffer frameDataUBO;
	frameDataUBO.Create(&base, sizeof(FrameUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VKBuffer dirLightUBO;
	dirLightUBO.Create(&base, sizeof(DirLightUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	// Create a large buffer which will hold the model matrices
	VKBuffer instanceDataBuffer;
	instanceDataBuffer.Create(&base, sizeof(glm::mat4) * 32, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VkDescriptorBufferInfo bufferInfo = {};
	bufferInfo.buffer = instanceDataBuffer.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	renderer->UpdateGlobalBuffersSet(bufferInfo, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

	bufferInfo.buffer = frameDataUBO.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	renderer->UpdateGlobalBuffersSet(bufferInfo, 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

	bufferInfo.buffer = dirLightUBO.GetBuffer();
	bufferInfo.offset = 0;
	bufferInfo.range = VK_WHOLE_SIZE;

	renderer->UpdateGlobalBuffersSet(bufferInfo, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

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


	// OFFSCREEN

	MaterialFeatures postQuadMatFeatures = {};
	postQuadMatFeatures.cullMode = VK_CULL_MODE_FRONT_BIT;
	postQuadMatFeatures.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	postQuadMatFeatures.enableDepthWrite = VK_TRUE;

	// Empty mesh to render the quad with no buffers
	Mesh postQuadMesh = {};
	postQuadMesh.vertexCount = 3;
	Material postQuadMat;
	postQuadMat.Create(renderer, postQuadMesh, postQuadMatFeatures, "Data/Shaders/post_quad_vert.spv", "Data/Shaders/post_quad_frag.spv", renderer->GetDefaultRenderPass());

	// SHADOW MAPPING

	Mesh shadowMesh = {};

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
	Material shadowMat;
	shadowMat.Create(renderer, shadowMesh, shadowMatFeatures, "Data/Shaders/shadow_vert.spv", "Data/Shaders/shadow_frag.spv", shadowFB.GetRenderPass());


	VkDescriptorSet quadSet = renderer->AllocateUserTextureDescriptorSet();
	renderer->UpdateUserTextureSet2D(quadSet, offscreenFB.GetFirstColorTexture(), 0);

	// Compute
	ComputeMaterial computeMat;
	computeMat.Create(renderer, "Data/Shaders/compute.spv");
	
	VkDescriptorSet computeSet = renderer->AllocateSetFromLayout(computeMat.GetSetLayout());

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

	VkSemaphore computeSemaphore;
	VkSemaphore graphicsSemaphore;

	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeSemaphore) != VK_SUCCESS)
	{
		std::cout << "Failed to create compute semaphore\n";
		return 1;
	}
	if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &graphicsSemaphore) != VK_SUCCESS)
	{
		std::cout << "Failed to create graphics semaphore\n";
		return 1;
	}


	// Signal the semaphore
	VkSubmitInfo semaInfo = {};
	semaInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	semaInfo.signalSemaphoreCount = 1;
	semaInfo.pSignalSemaphores = &graphicsSemaphore;

	if (vkQueueSubmit(base.GetGraphicsQueue(), 1, &semaInfo, VK_NULL_HANDLE) != VK_SUCCESS)
	{
		std::cout << "Failed to submit\n";
		return 1;
	}
	vkQueueWaitIdle(base.GetGraphicsQueue());

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkFence computeFence;

	if (vkCreateFence(device, &fenceInfo, nullptr, &computeFence) != VK_SUCCESS)
	{
		std::cout << "Failed to create fence\n";
		return 1;
	}


	const vkutils::QueueFamilyIndices& indices = base.GetQueueFamilyIndices();

	// Record the compute command buffer
	VkCommandBuffer computeCmdBuffer = renderer->CreateComputeCommandBuffer(true);


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
		VkImageSubresourceRange range = {};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.baseArrayLayer = 0;
		range.baseMipLevel = 0;
		range.layerCount = 1;
		range.levelCount = 1;

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
			return 1;
		}
		vkQueueWaitIdle(base.GetGraphicsQueue());		// Or wait for fence?

		renderer->FreeGraphicsCommandBuffer(cmd);
	}

	

	// Create quad to display the image create by the compute shader
	MaterialFeatures quadMatFeatures = {};
	quadMatFeatures.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	quadMatFeatures.cullMode = VK_CULL_MODE_BACK_BIT;
	quadMatFeatures.enableDepthWrite = VK_TRUE;

	Mesh quadMesh = MeshDefaults::CreateQuad(renderer);
	Material quadMat;
	quadMat.Create(renderer, quadMesh, quadMatFeatures, "Data/Shaders/quad_vert.spv", "Data/Shaders/quad_frag.spv", offscreenFB.GetRenderPass());

	// Create mipmaps

	VkCommandBuffer cmdBuffer = renderer->CreateGraphicsCommandBuffer(true);

	renderer->CreateMipMaps(cmdBuffer, modelManager.GetRenderModel(trashCanEntity).texture);
	renderer->CreateMipMaps(cmdBuffer, modelManager.GetRenderModel(floorEntity).texture);

	const std::vector<ParticleSystem>& particleSystems = particleManager.GetParticlesystems();

	for (size_t i = 0; i < particleSystems.size(); i++)
	{
		renderer->CreateMipMaps(cmdBuffer, particleSystems[i].GetTexture());
	}

	if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS)
	{
		std::cout << "Failed to end command buffer\n";
		return 1;
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	VkFence fence;

	fenceInfo.flags = 0;

	if (vkCreateFence(device, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
	{
		std::cout << "Failed to create fence\n";
		return 1;
	}
	if (vkQueueSubmit(base.GetGraphicsQueue(), 1, &submitInfo, fence) != VK_SUCCESS)
	{
		std::cout << "Failed to submit\n";
		return 1;
	}
	if (vkWaitForFences(device, 1, &fence, VK_TRUE, 100000000000) != VK_SUCCESS)
	{
		std::cout << "wait failed\n";
		return 1;
	}

	vkDestroyFence(device, fence, nullptr);

	renderer->FreeGraphicsCommandBuffer(cmdBuffer);

	float lastTime = 0.0f;
	float deltaTime = 0.0f;

	Camera camera;
	camera.SetPosition(glm::vec3(0.0f, 0.5f, 1.5f));
	camera.SetProjectionMatrix(85.0f, width, height, 0.1f, 20.0f);

	glm::mat4 previousFrameView = glm::mat4(1.0f);

	Camera lightSpaceCamera;
	lightSpaceCamera.SetProjectionMatrix(-5.0f, 5.0f, -5.0f, 5.0f, 0.1f, 10.0f);
	lightSpaceCamera.SetViewMatrix(glm::vec3(0.0f, 2.0f, 2.5f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	float timeElapsed = 0.0f;

	while (!glfwWindowShouldClose(window.GetHandle()))
	{
		window.UpdateInput();

		double currentTime = glfwGetTime();
		deltaTime = (float)currentTime - lastTime;
		lastTime = (float)currentTime;
		timeElapsed += deltaTime;

		camera.Update(deltaTime, true, true);

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto curTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(curTime - startTime).count();

		renderer->WaitForFrameFences();
		renderer->BeginCmdRecording();

		VkCommandBuffer cmdBuffer = renderer->GetCurrentCmdBuffer();

		VkPipelineLayout pipelineLayout = renderer->GetPipelineLayout();
		VkDescriptorSet globalTexturesSet = renderer->GetGlobalTexturesSet();

		
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &globalTexturesSet, 0, nullptr);	

		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;		
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		// SHADOW MAPPING

		viewport.width = shadowFB.GetWidth();
		viewport.height = shadowFB.GetHeight();

		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

		renderer->SetCamera(lightSpaceCamera);

		VkClearValue depthClearValue = {};
		depthClearValue.depthStencil = { 1.0f, 0 };

		renderer->BeginRenderPass(cmdBuffer, shadowFB, 1, &depthClearValue);
		modelManager.Render(cmdBuffer, pipelineLayout, shadowMat.GetPipeline());
		vkCmdEndRenderPass(cmdBuffer);

		// Set the normal camera
		renderer->SetCamera(camera);

		// Clouds low res pass
		volClouds.PerformCloudsPass(renderer, cmdBuffer);
		// Clouds reprojection pass
		volClouds.PerformCloudsReprojectionPass(renderer, cmdBuffer);

		// OFFSCREEN
		viewport.width = (float)surfaceExtent.width;
		viewport.height = (float)surfaceExtent.height;

		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

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

		renderer->BeginRenderPass(cmdBuffer, offscreenFB, 2, clearValues);
	
		modelManager.Render(cmdBuffer, pipelineLayout, VK_NULL_HANDLE);

		VkBuffer vertexBuffers[] = { VK_NULL_HANDLE };
		VkDeviceSize offsets[] = { 0 };

		vertexBuffers[0] = quadMesh.vb.GetBuffer();
		vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, quadMat.GetPipeline());
		vkCmdDraw(cmdBuffer, 6, 1, 0, 0);

		
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


		// Normal pass
		renderer->BeginDefaultRenderPass();

		vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, postQuadMat.GetPipeline());
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 2, 1, &quadSet, 0, nullptr);
		vkCmdDraw(cmdBuffer, (uint32_t)postQuadMesh.vertexCount, 1, 0, 0);

		renderer->EndDefaultRenderPass();
		renderer->EndCmdRecording();


		// Update buffers
		renderer->UpdateCameraUBO();

		const VolumetricCloudsData& volCloudsData = volClouds.GetVolumetricCloudsData();

		FrameUBO frameData = {};
		frameData.deltaTime = deltaTime;
		frameData.timeElapsed = timeElapsed;
		frameData.ambientBottomColor = volCloudsData.ambientBottomColor;
		frameData.ambientMult = volCloudsData.ambientMult;
		frameData.ambientTopColor = volCloudsData.ambientTopColor;
		frameData.cloudCoverage = volCloudsData.cloudCoverage;
		frameData.cloudStartHeight = volCloudsData.cloudStartHeight;
		frameData.cloudLayerThickness = volCloudsData.cloudLayerThickness;
		frameData.cloudLayerTopHeight = volCloudsData.cloudLayerTopHeight;
		frameData.timeScale = volCloudsData.timeScale;
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

		DirLightUBO dirLightData = {};
		dirLightData.lightSpaceMatrix[0] = lightSpaceCamera.GetProjectionMatrix() * lightSpaceCamera.GetViewMatrix();



		void* mapped = frameDataUBO.Map(device, 0, VK_WHOLE_SIZE);
		memcpy(mapped, &frameData, sizeof(FrameUBO));
		frameDataUBO.Unmap(device);

		mapped = dirLightUBO.Map(device, 0, VK_WHOLE_SIZE);
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

		particleManager.Update(device, deltaTime);


		// Compute
		
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
			return 1;
		}
		
		renderer->AcquireNextImage();
		renderer->Present(graphicsSemaphore, computeSemaphore);

		volClouds.EndFrame();
		previousFrameView = camera.GetViewMatrix();
	}

	vkDeviceWaitIdle(device);

	quadMat.Dispose(device);
	quadMesh.vb.Dispose(device);
	computeMat.Dispose(device);
	volClouds.Dispose(device);
	offscreenFB.Dispose(device);
	shadowFB.Dispose(device);
	instanceDataBuffer.Dispose(device);
	frameDataUBO.Dispose(device);
	dirLightUBO.Dispose(device);

	vkDestroyFence(device, computeFence, nullptr);
	storageTexture.Dispose(device);
	vkDestroySemaphore(device, computeSemaphore, nullptr);
	vkDestroySemaphore(device, graphicsSemaphore, nullptr);
	
	modelManager.Dispose(device);
	particleManager.Dispose(device);
	skybox.Dispose(device);
	
	postQuadMat.Dispose(device);
	shadowMat.Dispose(device);

	transformManager.Dispose();
	renderer->Dispose();
	delete renderer;

	glfwTerminate();

	return 0;
}