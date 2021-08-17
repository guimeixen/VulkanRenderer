#include "Water.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/matrix_access.hpp"

Water::Water()
{
	waterTopPlane = {};
	waterBottomPlane = {};
	waterHeight = 0.0f;
	viewFrame = glm::mat4(1.0f);
	gridResolution = 128;
	indexCount = 0;
	set = VK_NULL_HANDLE;
}

bool Water::Load(VKRenderer* renderer, VkRenderPass renderPass)
{
	VKBase& base = renderer->GetBase();
	VkDevice device = base.GetDevice();

	SetWaterHeight(waterHeight);

	/*VertexAttribute attrib = {};
	// UV
	attrib.count = 2;
	attrib.vertexAttribFormat = VertexAttributeFormat::FLOAT;
	attrib.offset = 0;

	VertexInputDesc desc = {};
	desc.attribs = { attrib };
	desc.stride = 2 * sizeof(float);*/

	std::vector<glm::vec2> vertices(gridResolution * gridResolution);
	std::vector<unsigned short> indices((gridResolution - 1) * (gridResolution - 1) * 6);

	unsigned int v = 0;
	for (unsigned int z = 0; z < gridResolution; z++)
	{
		for (unsigned int x = 0; x < gridResolution; x++)
		{
			vertices[v++] = glm::vec2((float)x / (gridResolution - 1), (float)z / (gridResolution - 1));
		}
	}

	unsigned int index = 0;
	for (unsigned int z = 0; z < gridResolution - 1; z++)
	{
		for (unsigned int x = 0; x < gridResolution - 1; x++)
		{
			unsigned short topLeft = z * (unsigned short)gridResolution + x;
			unsigned short topRight = topLeft + 1;
			unsigned short bottomLeft = (z + 1) * (unsigned short)gridResolution + x;
			unsigned short bottomRight = bottomLeft + 1;

			indices[index++] = topLeft;
			indices[index++] = bottomLeft;
			indices[index++] = bottomRight;

			indices[index++] = topLeft;
			indices[index++] = bottomRight;
			indices[index++] = topRight;
		}
	}

	indexCount = indices.size();

	VKBuffer vertexStagingBuffer, indexStagingBuffer;

	vertexStagingBuffer.Create(&base, vertices.size() * sizeof(glm::vec2), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	indexStagingBuffer.Create(&base, indices.size() * sizeof(unsigned short), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	unsigned int vertexSize = vertexStagingBuffer.GetSize();
	unsigned int indexSize = indexStagingBuffer.GetSize();

	void* mapped = vertexStagingBuffer.Map(device, 0, vertexSize);
	memcpy(mapped, vertices.data(), (size_t)vertexSize);
	vertexStagingBuffer.Unmap(device);

	mapped = indexStagingBuffer.Map(device, 0, indexSize);
	memcpy(mapped, indices.data(), (size_t)indexSize);
	indexStagingBuffer.Unmap(device);

	vb.Create(&base, vertices.size() * sizeof(glm::vec2), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	ib.Create(&base, indices.size() * sizeof(unsigned short), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	base.CopyBuffer(vertexStagingBuffer, vb, vertexSize);
	base.CopyBuffer(indexStagingBuffer, ib, indexSize);

	vertexStagingBuffer.Dispose(device);
	indexStagingBuffer.Dispose(device);

	/*mat = renderer->CreateMaterialInstanceFromBaseMat(scriptManager, "Data/Resources/Materials/water_disp_mat.lua", projectedGridMesh.vao->GetVertexInputDescs());

	if (mat && mat->textures.size() > 0)
	{
		TextureParams params = { TextureWrap::REPEAT, TextureFilter::LINEAR,TextureFormat::RGBA, TextureInternalFormat::RGBA8, TextureDataType::UNSIGNED_BYTE, true, false };
		normalMap = renderer->CreateTexture2D("Data/Resources/Textures/oceanwaves_ddn2.png", params);
		mat->textures[1] = normalMap;
		foamTexture = renderer->CreateTexture2D("Data/Resources/Textures/foam.png", params);
		mat->textures[4] = foamTexture;

		renderer->UpdateMaterialInstance(mat);
	}
	*/

	// Create the water pipeline

	PipelineInfo pipeInfo = VKPipeline::DefaultFillStructs();

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = base.GetSurfaceExtent();

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
	};

	VkVertexInputBindingDescription bindingDesc = {};
	bindingDesc.binding = 0;
	bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindingDesc.stride = sizeof(glm::vec2);

	VkVertexInputAttributeDescription attribDesc = {};
	attribDesc.binding = 0;
	attribDesc.format = VK_FORMAT_R32G32_SFLOAT;
	attribDesc.location = 0;
	attribDesc.offset = 0;

	// No need to set the viewport because it's dynamic
	pipeInfo.viewportState.viewportCount = 1;
	pipeInfo.viewportState.scissorCount = 1;
	pipeInfo.viewportState.pScissors = &scissor;

	pipeInfo.colorBlending.attachmentCount = 1;
	pipeInfo.colorBlending.pAttachments = &colorBlendAttachment;

	pipeInfo.dynamicState.dynamicStateCount = 1;
	pipeInfo.dynamicState.pDynamicStates = dynamicStates;

	pipeInfo.vertexInput.vertexBindingDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexBindingDescriptions = &bindingDesc;
	pipeInfo.vertexInput.vertexAttributeDescriptionCount = 1;
	pipeInfo.vertexInput.pVertexAttributeDescriptions = &attribDesc;

	vertexShader.LoadShader(device, "water_disp", VK_SHADER_STAGE_VERTEX_BIT);
	fragmentShader.LoadShader(device, "water_disp", VK_SHADER_STAGE_FRAGMENT_BIT);

	if (!pipeline.Create(device, pipeInfo, renderer->GetPipelineLayout(), vertexShader, fragmentShader, renderPass))
		return false;

	return true;
}

void Water::Update(const Camera& camera, float deltaTime)
{
	intersectionPoints.clear();

	glm::vec3 camPos = camera.GetPosition();

	float range = std::max(0.0f, 10.0f) + 5.0f;

	if (camPos.y < waterHeight)
		camPos.y = std::min(camPos.y, waterHeight - range);
	else
		camPos.y = std::max(camPos.y, waterHeight + range);

	const float scale = 18.0f;
	glm::vec3 focus = camera.GetPosition() + camera.GetForward() * scale;
	focus.y = waterHeight;

	viewFrame = glm::lookAt(camPos, focus, glm::vec3(0.0f, 1.0f, 0.0f));

	// Construct view and projection matrices
	glm::mat4 projectorViewProj = camera.GetProjectionMatrix() * viewFrame;

	const FrustumCorners& frustumCorners = camera.GetFrustum().GetCorners();
	frustumCornersWorld[0] = frustumCorners.nbl;
	frustumCornersWorld[1] = frustumCorners.ntl;
	frustumCornersWorld[2] = frustumCorners.ntr;
	frustumCornersWorld[3] = frustumCorners.nbr;

	frustumCornersWorld[4] = frustumCorners.fbl;
	frustumCornersWorld[5] = frustumCorners.ftl;
	frustumCornersWorld[6] = frustumCorners.ftr;
	frustumCornersWorld[7] = frustumCorners.fbr;

	range = std::max(1.0f, 10.0f);

	// For each corner if its world space position is  
	// between the wave range then add it to the list.
	for (size_t i = 0; i < 8; i++)
	{
		if (frustumCornersWorld[i].y <= waterHeight + range && frustumCornersWorld[i].y >= waterHeight - range)
		{
			intersectionPoints.push_back(frustumCornersWorld[i]);
		}
	}

	static const int frustumIndices[12][2] = {
		{ 0,1 },
	{ 1,2 },
	{ 2,3 },
	{ 3,0 },
	{ 4,5 },
	{ 5,6 },
	{ 6,7 },
	{ 7,4 },
	{ 0,4 },
	{ 1,5 },
	{ 2,6 },
	{ 3,7 }
	};

	// Now take each segment in the frustum box and check
	// to see if it intersects the ocean plane on both the
	// upper and lower ranges.
	for (size_t i = 0; i < 12; i++)
	{
		glm::vec3 p0 = frustumCornersWorld[frustumIndices[i][0]];
		glm::vec3 p1 = frustumCornersWorld[frustumIndices[i][1]];

		glm::vec3 max, min;
		if (SegmentPlaneIntersection(p0, p1, glm::vec3(0.0f, 1.0f, 0.0f), waterHeight + range, max))
		{
			intersectionPoints.push_back(max);
		}

		if (SegmentPlaneIntersection(p0, p1, glm::vec3(0.0f, 1.0f, 0.0f), waterHeight - range, min))
		{
			intersectionPoints.push_back(min);
		}
	}

	float xmin = std::numeric_limits<float>::max();
	float ymin = std::numeric_limits<float>::max();
	float xmax = std::numeric_limits<float>::min();
	float ymax = std::numeric_limits<float>::min();
	glm::vec4 q = glm::vec4(0.0f);
	glm::vec4 p = glm::vec4(0.0f);

	// Now convert each world space position into
	// projector screen space. The min/max x/y values
	// are then used for the range conversion matrix.
	// Calculate the x and y span of vVisible in projector space
	for (size_t i = 0; i < intersectionPoints.size(); i++)
	{
		// Project the points of intersection between the frustum and the waterTop or waterBottom plane to the waterPlane
		q.x = intersectionPoints[i].x;
		q.y = waterHeight;
		q.z = intersectionPoints[i].z;
		q.w = 1.0f;

		// Now transform the points to projector space
		p = projectorViewProj * q;
		p.x /= p.w;
		p.y /= p.w;

		if (p.x < xmin) xmin = p.x;
		if (p.y < ymin) ymin = p.y;
		if (p.x > xmax) xmax = p.x;
		if (p.y > ymax) ymax = p.y;
	}

	// Create a matrix that transform the [0,1] range to [xmin,xmax] and [ymin,ymax] and leave the z and w intact
	glm::mat4 rangeMap;
	rangeMap = glm::row(rangeMap, 0, glm::vec4(xmax - xmin, 0.0f, 0.0f, xmin));
	rangeMap = glm::row(rangeMap, 1, glm::vec4(0.0f, ymax - ymin, 0.0f, ymin));
	rangeMap = glm::row(rangeMap, 2, glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
	rangeMap = glm::row(rangeMap, 3, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

	// Now update the projector matrix with the range conversion matrix
	glm::mat4 projectorToWorld = glm::inverse(projectorViewProj) * rangeMap;

	glm::vec2 ndcCorners[4];
	ndcCorners[0] = glm::vec2(0.0f, 0.0f);
	ndcCorners[1] = glm::vec2(1.0f, 0.0f);
	ndcCorners[2] = glm::vec2(1.0f, 1.0f);
	ndcCorners[3] = glm::vec2(0.0f, 1.0f);

	// Now transform the corners of the 
	for (int i = 0; i < 4; i++)
	{
		glm::vec4 a, b;

		// Transform the ndc corners to world space
		a = projectorToWorld * glm::vec4(ndcCorners[i].x, ndcCorners[i].y, -1.0f, 1.0f);
		b = projectorToWorld * glm::vec4(ndcCorners[i].x, ndcCorners[i].y, 1.0f, 1.0f);

		// And calculate the intersection between the line made by this two points and the water plane
		// in homogeneous space
		// The rest of the grid vertices will then be interpolated in the vertex shader
		float h = waterHeight;

		glm::vec4 ab = b - a;

		float t = (a.w * h - a.y) / (ab.y - ab.w * h);

		viewCorners[i] = a + ab * t;
	}

	/*float nAngle0 = 42.0f * (3.14159f / 180.0f);
	float nAngle1 = 76.0f * (3.14159f / 180.0f);
	normalMapOffset0 += glm::vec2(glm::cos(nAngle0), glm::sin(nAngle0)) * 0.025f * deltaTime;
	normalMapOffset1 += glm::vec2(glm::cos(nAngle1), glm::sin(nAngle1)) * 0.010f * deltaTime;*/
}

void Water::Render(VkCommandBuffer cmdBuffer, VkPipelineLayout pipelineLayout)
{
	VkBuffer vertexBuffers[] = { vb.GetBuffer() };
	VkDeviceSize offsets[] = { 0 };

	vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());
	vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
	vkCmdBindIndexBuffer(cmdBuffer, ib.GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
	//vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, USER_TEXTURES_SET_BINDING, 1, &set, 0, nullptr);
	vkCmdDrawIndexed(cmdBuffer, indexCount, 1, 0, 0, 0);
}

void Water::Dispose(VkDevice device)
{
	vertexShader.Dispose(device);
	fragmentShader.Dispose(device);
	pipeline.Dispose(device);
	vb.Dispose(device);
	ib.Dispose(device);
}

void Water::SetWaterHeight(float height)
{
	waterHeight = height;
	waterBottomPlane.SetNormalAndPoint(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, waterHeight, 0.0f));
	waterTopPlane.SetNormalAndPoint(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, waterHeight + 1.315f, 0.0f));
}

void Water::IntersectFrustumWithWaterPlane(const Camera& camera)
{
	const FrustumCorners& corners = camera.GetFrustum().GetCorners();

	IntersectFrustumEdgeWaterPlane(corners.nbl, corners.ntl);
	IntersectFrustumEdgeWaterPlane(corners.ntl, corners.ntr);
	IntersectFrustumEdgeWaterPlane(corners.ntr, corners.nbr);
	IntersectFrustumEdgeWaterPlane(corners.nbr, corners.nbl);

	IntersectFrustumEdgeWaterPlane(corners.fbl, corners.ftl);
	IntersectFrustumEdgeWaterPlane(corners.ftl, corners.ftr);
	IntersectFrustumEdgeWaterPlane(corners.ftr, corners.fbr);
	IntersectFrustumEdgeWaterPlane(corners.fbr, corners.fbl);

	IntersectFrustumEdgeWaterPlane(corners.nbl, corners.fbl);
	IntersectFrustumEdgeWaterPlane(corners.ntl, corners.ftl);
	IntersectFrustumEdgeWaterPlane(corners.ntr, corners.ftr);
	IntersectFrustumEdgeWaterPlane(corners.nbr, corners.fbr);
}

void Water::IntersectFrustumEdgeWaterPlane(const glm::vec3& start, const glm::vec3& end)
{
	glm::vec3 delta = end - start;
	glm::vec3 dir = glm::normalize(delta);
	float length = glm::length(delta);

	float distance = 0.0f;

	if (waterTopPlane.IntersectRay(start, dir, distance))
	{
		if (distance <= length)
		{
			glm::vec3 hitPos = start + dir * distance;

			intersectionPoints.push_back(glm::vec3(hitPos.x, waterHeight, hitPos.z));
		}
	}

	if (waterBottomPlane.IntersectRay(start, dir, distance))
	{
		if (distance <= length)
		{
			glm::vec3 hitPos = start + dir * distance;

			intersectionPoints.push_back(glm::vec3(hitPos.x, waterHeight, hitPos.z));
		}
	}
}

bool Water::SegmentPlaneIntersection(const glm::vec3& a, const glm::vec3& b, const glm::vec3& n, float d, glm::vec3& q)
{
	glm::vec3 ab = b - a;
	float t = (d - glm::dot(n, a)) / glm::dot(n, ab);

	if (t > -0.0f && t <= 1.0f)
	{
		q = a + t * ab;
		return true;
	}

	return false;
}
