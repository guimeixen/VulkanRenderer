#include "Model.h"
#include "VKBuffer.h"
#include "VertexTypes.h"

#include "assimp/scene.h"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"

#include <vector>
#include <iostream>

Model::Model()
{
	indexCount = 0;
}

bool Model::Load(VKBase& base, const std::string& path)
{
	Assimp::Importer importer;
	const aiScene* aiscene = importer.ReadFile(path, aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs); //| aiProcess_GenSmoothNormals); //| aiProcess_CalcTangentSpace);

	if (!aiscene || aiscene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !aiscene->mRootNode)
	{
		std::cout << "Failed to load model. Assimp Error: " << importer.GetErrorString() << '\n';
		return false;
	}

	std::vector<unsigned short> indices;
	std::vector<Vertex> vertices;

	// Load all the model meshes
	for (unsigned int i = 0; i < aiscene->mNumMeshes; i++)
	{
		const aiMesh* aimesh = aiscene->mMeshes[i];

		for (unsigned int j = 0; j < aimesh->mNumFaces; j++)
		{
			aiFace face = aimesh->mFaces[j];

			for (unsigned int k = 0; k < face.mNumIndices; k++)
			{
				indices.push_back(face.mIndices[k]);
			}
		}


		vertices.resize(aimesh->mNumVertices);

		for (unsigned int j = 0; j < aimesh->mNumVertices; j++)
		{
			Vertex& v = vertices[j];

			v.pos = glm::vec3(aimesh->mVertices[j].x, aimesh->mVertices[j].y, aimesh->mVertices[j].z);
			v.normal = glm::vec3(aimesh->mNormals[j].x, aimesh->mNormals[j].y, aimesh->mNormals[j].z);

			if (aimesh->mTextureCoords[0])
			{
				// A vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
				// use models where a vertex can have multiple texture coordinates so we always take the first set (0).
				v.uv = glm::vec2(aimesh->mTextureCoords[0][j].x, aimesh->mTextureCoords[0][j].y);
			}
			else
			{
				v.uv = glm::vec2(0.0f, 0.0f);
			}
		}

		indexCount = static_cast<unsigned short>(indices.size());

		VKBuffer vertexStagingBuffer, indexStagingBuffer;

		VkDevice device = base.GetDevice();

		vertexStagingBuffer.Create(&base, vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		indexStagingBuffer.Create(&base, indices.size() * sizeof(unsigned short), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		unsigned int vertexSize = vertexStagingBuffer.GetSize();

		void* data;
		vkMapMemory(device, vertexStagingBuffer.GetBufferMemory(), 0, vertexSize, 0, &data);
		memcpy(data, vertices.data(), (size_t)vertexSize);
		vkUnmapMemory(device, vertexStagingBuffer.GetBufferMemory());

		unsigned int indexSize = indexStagingBuffer.GetSize();
		vkMapMemory(device, indexStagingBuffer.GetBufferMemory(), 0, indexSize, 0, &data);
		memcpy(data, indices.data(), (size_t)indexSize);
		vkUnmapMemory(device, indexStagingBuffer.GetBufferMemory());

		vertexBuffer.Create(&base, vertices.size() * sizeof(Vertex), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		indexBuffer.Create(&base, indices.size() * sizeof(unsigned short), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		base.CopyBuffer(vertexStagingBuffer, vertexBuffer, vertexSize);
		base.CopyBuffer(indexStagingBuffer, indexBuffer, indexSize);

		vertexStagingBuffer.Dispose(device);
		indexStagingBuffer.Dispose(device);
	}

	return true;
}

void Model::Dispose(VkDevice device)
{
	vertexBuffer.Dispose(device);
	indexBuffer.Dispose(device);
}
