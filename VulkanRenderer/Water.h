#pragma once

#include "VKRenderer.h"
#include "VKShader.h"
#include "VKPipeline.h"
#include "Frustum.h"

class Water
{
public:
	Water();

	bool Load(VKRenderer* renderer, VkRenderPass renderPass);
	void Update(const Camera& camera, float deltaTime);
	void Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout);
	void Dispose(VkDevice device);

	void SetWaterHeight(float height);

	VkDescriptorSet GetDescriptorSet() const { return set; }

	const glm::mat4& GetViewFrame() const { return viewFrame; }
	const glm::vec4& GetViewCorner0() const { return viewCorners[0]; }
	const glm::vec4& GetViewCorner1() const { return viewCorners[1]; }
	const glm::vec4& GetViewCorner2() const { return viewCorners[2]; }
	const glm::vec4& GetViewCorner3() const { return viewCorners[3]; }

private:
	void IntersectFrustumWithWaterPlane(const Camera& camera);
	void IntersectFrustumEdgeWaterPlane(const glm::vec3& start, const glm::vec3& end);
	bool SegmentPlaneIntersection(const glm::vec3& a, const glm::vec3& b, const glm::vec3& n, float d, glm::vec3& q);

private:
	VkDescriptorSet set;
	VKShader vertexShader;
	VKShader fragmentShader;
	VKPipeline pipeline;
	VKBuffer vb;
	VKBuffer ib;
	uint32_t indexCount;

	std::vector<glm::vec3> intersectionPoints;
	float waterHeight;
	Plane waterTopPlane;
	Plane waterBottomPlane;
	glm::vec4 viewCorners[4];
	glm::mat4 viewFrame;
	glm::vec3 frustumCornersWorld[8];
	unsigned int gridResolution;
};