#include "Window.h"
#include "Input.h"
#include "Random.h"
#include "Camera.h"
#include "ParticleManager.h"
#include "ModelManager.h"
#include "UniformBufferTypes.h"
#include "EntityManager.h"
#include "TransformManager.h"
#include "Allocator.h"
#include "RenderingPath.h"

#include "glm/gtc/matrix_transform.hpp"

#include <iostream>

int main()
{
	const unsigned int width = 960;
	const unsigned int height = 540;

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
	
	RenderingPath renderingPath;
	renderingPath.Init(renderer, width, height);

	const VKTexture2D& storageTexture = renderingPath.GetStorageTexture();
	
	ModelManager modelManager;
	if (!modelManager.Init(renderer, renderingPath.GetHDRFramebuffer().GetRenderPass()))
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
	
	ParticleManager particleManager;
	if (!particleManager.Init(renderer, renderingPath.GetHDRFramebuffer().GetRenderPass()))
		return 1;

	if (!particleManager.AddParticleSystem(renderer, "Data/Textures/particleTexture.png", 10))
	{
		std::cout << "Failed to add particle system\n";
		return 1;
	}

	renderingPath.PerformComputePass();

	// Create mipmaps
	VkCommandBuffer cmdBuffer = renderer->BeginMipMaps();
	renderer->CreateMipMaps(cmdBuffer, modelManager.GetRenderModel(trashCanEntity).texture);
	renderer->CreateMipMaps(cmdBuffer, modelManager.GetRenderModel(floorEntity).texture);

	const std::vector<ParticleSystem>& particleSystems = particleManager.GetParticlesystems();

	for (size_t i = 0; i < particleSystems.size(); i++)
	{
		renderer->CreateMipMaps(cmdBuffer, particleSystems[i].GetTexture());
	}

	if (!renderer->EndMipMaps(cmdBuffer))
		return 1;

	float lastTime = 0.0f;
	float deltaTime = 0.0f;

	Camera camera;
	camera.SetPosition(glm::vec3(0.0f, 0.5f, 1.5f));
	camera.SetProjectionMatrix(75.0f, width, height, 0.5f, 1000.0f);

	glm::mat4 previousFrameView = glm::mat4(1.0f);

	float timeElapsed = 0.0f;

	while (!glfwWindowShouldClose(window.GetHandle()))
	{
		window.UpdateInput();

		double currentTime = glfwGetTime();
		deltaTime = (float)currentTime - lastTime;
		lastTime = (float)currentTime;
		timeElapsed += deltaTime;

		camera.Update(deltaTime, true, true);
		renderingPath.Update(camera, deltaTime);

		renderer->WaitForFrameFences();
		renderer->BeginCmdRecording();

		unsigned int currentFrame = renderer->GetCurrentFrame();
		VkCommandBuffer cmdBuffer = renderer->GetCurrentCmdBuffer();
		renderer->BeginQuery();

		VkPipelineLayout pipelineLayout = renderer->GetPipelineLayout();
		VkDescriptorSet globalBuffersSet = renderer->GetGlobalBuffersSet();
		VkDescriptorSet globalTexturesSet = renderer->GetGlobalTexturesSet();
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, GLOBAL_BUFFER_SET_BINDING, 1, &globalBuffersSet, 0, nullptr);
		vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, GLOBAL_TEXTURES_SET_BINDING, 1, &globalTexturesSet, 0, nullptr);

		renderingPath.PerformShadowMapPass(cmdBuffer, modelManager);
		renderer->SetCamera(camera);
		renderingPath.PerformVolumetricCloudsPass(cmdBuffer);
		renderingPath.PerformHDRPass(cmdBuffer, modelManager, particleManager);
		renderingPath.PerformPostProcessPass(cmdBuffer);
		renderer->EndQuery();
		renderer->EndCmdRecording();

		// Update buffers
		renderer->UpdateCameraUBO();
		renderingPath.UpdateBuffers(camera, modelManager, transformManager, deltaTime, timeElapsed);
		particleManager.Update(device, deltaTime);

		// Compute	
		renderingPath.SubmitCompute();		

		renderer->AcquireNextImage();
		renderer->Present(renderingPath.GetGraphicsSemaphore(), renderingPath.GetComputeSemaphore());
		renderingPath.EndFrame(camera);
	}

	vkDeviceWaitIdle(device);

	renderingPath.Dispose();	
	modelManager.Dispose(device);
	particleManager.Dispose(device);
	transformManager.Dispose();
	renderer->Dispose();
	delete renderer;

	glfwTerminate();

	return 0;
}