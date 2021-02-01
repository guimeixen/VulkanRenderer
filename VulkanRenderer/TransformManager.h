#pragma once

#include "EntityManager.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include <unordered_map>

class Transform;
class Allocator;

struct TransformInstanceData
{
	unsigned int size;
	unsigned int capacity;
	unsigned char* buffer;

	glm::mat4* localToWorld;
	glm::vec3* localPosition;
	glm::quat* localRotation;
	glm::vec3* localScale;
	Entity* parent;
	Entity* firstChild;
	Entity* prevSibling;
	Entity* nextSibling;
	bool* modified;
};

struct ModifiedTransform
{
	Entity e;
	const glm::mat4* localToWorld;
};

class TransformManager
{
public:
	void Init(Allocator* allocator, unsigned int initialCapacity);
	void Dispose();
	void ClearModifiedTransforms();

	void AddTransform(Entity e);
	void DuplicateTransform(Entity e, Entity newE);
	void RemoveTransform(Entity e);

	void SetParent(Entity e, Entity parent);
	void RemoveParent(Entity e);
	void SetLocalPosition(Entity e, const glm::vec3& position);
	void SetLocalRotation(Entity e, const glm::quat& rotation);
	void SetLocalRotationEuler(Entity e, const glm::vec3& euler);
	void SetLocalScale(Entity e, const glm::vec3& scale);
	void SetLocalToWorld(Entity e, const glm::mat4& localToWorld);
	void SetLocalToParent(Entity e, const glm::mat4& localToParent);
	void Rotate(Entity e, const glm::vec3& rot);

	const glm::mat4& GetLocalToWorld(Entity e) const;

	bool HasParent(Entity e) const;
	bool HasChildren(Entity e) const;

	// Don't return const otherwise it will not be possible to change them through script
	glm::vec3& GetLocalPosition(Entity e) const { return instanceData.localPosition[e.id]; }
	glm::quat& GetLocalRotation(Entity e) const { return instanceData.localRotation[e.id]; }
	glm::vec3& GetLocalScale(Entity e) const { return instanceData.localScale[e.id]; }

	glm::vec3 GetWorldPosition(Entity e) const { return instanceData.localToWorld[e.id][3]; }

	Entity GetParent(Entity e);
	Entity GetFirstChild(Entity e);
	Entity GetNextSibling(Entity e);

	unsigned int GetNumModifiedTransforms() const { return numModifiedTransforms; }
	const ModifiedTransform* GetModifiedTransforms() const { return modifiedTransforms; }

private:
	void CalcTransform(Entity e);
	void CalcTransform(Entity e, const glm::mat4& parent);

private:
	Allocator* allocator;
	bool isInit = false;
	TransformInstanceData instanceData;
	static const unsigned int MAX_MODIFIED_TRANSFORMS = 4096;
	unsigned int numModifiedTransforms = 0;
	ModifiedTransform modifiedTransforms[MAX_MODIFIED_TRANSFORMS];
};

