#include "TransformManager.h"

#include "Log.h"
#include "Allocator.h"

#include "glm/gtc/matrix_transform.hpp"

void TransformManager::Init(Allocator* allocator, unsigned int initialCapacity)
{
	if (isInit)
		return;

	this->allocator = allocator;

	instanceData = {};
	//instanceData.buffer = new unsigned char[initialCapacity * (sizeof(glm::mat4) + sizeof(glm::vec3) + sizeof(glm::quat) + sizeof(glm::vec3) + sizeof(Entity) * 4 + sizeof(bool))];
	instanceData.buffer = (unsigned char*)allocator->Allocate(initialCapacity * (sizeof(glm::mat4) + sizeof(glm::vec3) + sizeof(glm::quat) + sizeof(glm::vec3) + sizeof(Entity) * 4 + sizeof(bool)));
	instanceData.capacity = initialCapacity;
	instanceData.size = 0;

	instanceData.localToWorld = (glm::mat4*)instanceData.buffer;
	instanceData.localPosition = (glm::vec3*)(instanceData.localToWorld + initialCapacity);
	instanceData.localRotation = (glm::quat*)(instanceData.localPosition + initialCapacity);
	instanceData.localScale = (glm::vec3*)(instanceData.localRotation + initialCapacity);
	instanceData.parent = (Entity*)(instanceData.localScale + initialCapacity);
	instanceData.firstChild = (Entity*)(instanceData.parent + initialCapacity);
	instanceData.prevSibling = (Entity*)(instanceData.firstChild + initialCapacity);
	instanceData.nextSibling = (Entity*)(instanceData.prevSibling + initialCapacity);
	instanceData.modified = (bool*)(instanceData.nextSibling + initialCapacity);

	isInit = true;

	Log::Print(LogLevel::LEVEL_INFO, "Init Transform manager\n");
}

void TransformManager::Dispose()
{
	if (instanceData.buffer)
		allocator->Free(instanceData.buffer);

	Log::Print(LogLevel::LEVEL_INFO, "Disposing Transform manager\n");
}

void TransformManager::ClearModifiedTransforms()
{
	numModifiedTransforms = 0;

	// Try memset 0 to put everything to false
	for (unsigned int i = 0; i < instanceData.size; i++)
	{
		instanceData.modified[i] = false;
	}
}

void TransformManager::AddTransform(Entity e)
{
	// Only one transform per entity
	if (e.id < instanceData.size)
		return;

	if (instanceData.size == instanceData.capacity)
	{
		// resize
	}

	instanceData.localToWorld[instanceData.size] = glm::mat4(1.0f);
	instanceData.localPosition[instanceData.size] = glm::vec3(0.0f);
	instanceData.localRotation[instanceData.size] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	instanceData.localScale[instanceData.size] = glm::vec3(1.0f);
	instanceData.parent[instanceData.size] = { std::numeric_limits<unsigned int>::max() };
	instanceData.firstChild[instanceData.size] = { std::numeric_limits<unsigned int>::max() };
	instanceData.prevSibling[instanceData.size] = { std::numeric_limits<unsigned int>::max() };
	instanceData.nextSibling[instanceData.size] = { std::numeric_limits<unsigned int>::max() };

	instanceData.size++;
}

void TransformManager::DuplicateTransform(Entity e, Entity newE)
{
	if (instanceData.size == instanceData.capacity)
	{
		// resize
	}

	glm::mat4 localToWorld = instanceData.localToWorld[e.id];
	glm::vec3 worldPos = localToWorld[3];
	glm::vec3 worldScale = glm::vec3(glm::length(localToWorld[0]), glm::length(localToWorld[1]), glm::length(localToWorld[2]));

	// Normalize the scale otherwise when extracing the rotation, it will be wrong
	localToWorld[0] = glm::normalize(localToWorld[0]);
	localToWorld[1] = glm::normalize(localToWorld[1]);
	localToWorld[2] = glm::normalize(localToWorld[2]);
	glm::quat worldRot = glm::quat_cast(localToWorld);
	worldRot = glm::normalize(worldRot);


	instanceData.localToWorld[instanceData.size] = instanceData.localToWorld[e.id];
	/*instanceData.localPosition[instanceData.size] = instanceData.localPosition[e.id];
	instanceData.localRotation[instanceData.size] = instanceData.localRotation[e.id];
	instanceData.localScale[instanceData.size] = instanceData.localScale[e.id];*/
	instanceData.localPosition[instanceData.size] = worldPos;
	instanceData.localRotation[instanceData.size] = worldRot;
	instanceData.localScale[instanceData.size] = worldScale;

	instanceData.parent[instanceData.size] = { std::numeric_limits<unsigned int>::max() };
	instanceData.firstChild[instanceData.size] = { std::numeric_limits<unsigned int>::max() };
	instanceData.prevSibling[instanceData.size] = { std::numeric_limits<unsigned int>::max() };
	instanceData.nextSibling[instanceData.size] = { std::numeric_limits<unsigned int>::max() };

	/*if (instanceData.modified[instanceData.size] == false)
	{
		ModifiedTransform &mt = modifiedTransforms[numModifiedTransforms];
		mt.e = e;
		mt.localToWorld = &instanceData.localToWorld[e.id];
		numModifiedTransforms++;
	}*/

	instanceData.size++;
}

void TransformManager::SetParent(Entity e, Entity parent)
{
	Entity oldParent = instanceData.parent[e.id];

	// Remove the instance from the parent children list
	if (oldParent.IsValid())
	{
		Entity prevSibling = instanceData.prevSibling[e.id];
		Entity nextSibling = instanceData.nextSibling[e.id];

		if (instanceData.firstChild[oldParent.id].id == e.id)
			instanceData.firstChild[oldParent.id] = nextSibling;
		else
		{
			assert(prevSibling.IsValid());

			// We're going to remove the instance between prev and next. Now the next of the prev is the next sibling
			instanceData.nextSibling[prevSibling.id] = nextSibling;
		}

		// If the instance had a next sibling now we also need to fix the prev sibling of the next sibling. The previous sibling will now be the previous sibling of the instance we removed.
		if (nextSibling.IsValid())
			instanceData.prevSibling[nextSibling.id] = prevSibling;
	}

	// Add the instance to the new parent children list

	instanceData.parent[e.id] = parent;

	Entity firstChild = instanceData.firstChild[parent.id];

	instanceData.firstChild[parent.id] = e;
	instanceData.nextSibling[e.id] = firstChild;

	if (firstChild.IsValid())
		instanceData.prevSibling[firstChild.id] = e;


	glm::mat4 parentT = instanceData.localToWorld[parent.id];

	// Update the child local position, rotation and scale to be relative to the parent
	glm::mat4 t = glm::inverse(instanceData.localToWorld[parent.id]) * instanceData.localToWorld[e.id];

	instanceData.localPosition[e.id] = t[3];
	instanceData.localScale[e.id] = glm::vec3(glm::length(t[0]), glm::length(t[1]), glm::length(t[2]));

	glm::quat rotation = glm::inverse(instanceData.localRotation[parent.id]) * instanceData.localRotation[e.id];
	rotation = glm::normalize(rotation);
	instanceData.localRotation[e.id] = rotation;

	CalcTransform(e, parentT);
}

void TransformManager::RemoveParent(Entity e)
{
	if (!HasParent(e))
		return;

	Entity parent = instanceData.parent[e.id];
	Entity prevSibling = instanceData.prevSibling[e.id];
	Entity nextSibling = instanceData.nextSibling[e.id];

	if (instanceData.firstChild[parent.id].id == e.id)
		instanceData.firstChild[parent.id] = nextSibling;
	else
	{
		assert(prevSibling.IsValid());

		// We're going to remove the instance between prev and next. Now the next of the prev is the next sibling
		instanceData.nextSibling[prevSibling.id] = nextSibling;
	}

	// If the instance had a next sibling now we also need to fix the prev sibling of the next sibling. The previous sibling will now be the previous sibling of the instance we removed.
	if (nextSibling.IsValid())
		instanceData.prevSibling[nextSibling.id] = prevSibling;

	instanceData.parent[e.id] = { std::numeric_limits<unsigned int>::max() };

	// When removing the child, she goes back to world space so set the new local position and rotation (which are now in world space)
	glm::mat4 t = instanceData.localToWorld[e.id];

	instanceData.localPosition[e.id] = t[3];
	instanceData.localScale[e.id] = glm::vec3(glm::length(t[0]), glm::length(t[1]), glm::length(t[2]));

	// We need a rotation matrix to convert to quaternion
	// There's no need to remove the translation because it's going to be casted to a 3x3 matrix so it's going to be dropped
	// Normalize scale
	t[0] = glm::normalize(t[0]);
	t[1] = glm::normalize(t[1]);
	t[2] = glm::normalize(t[2]);
	glm::quat q = glm::quat_cast(t);
	q = glm::normalize(q);

	instanceData.localRotation[e.id] = q;

	CalcTransform(e, glm::mat4(1.0f));
}

void TransformManager::SetLocalPosition(Entity e, const glm::vec3& position)
{
	instanceData.localPosition[e.id] = position;
	CalcTransform(e);
}

void TransformManager::SetLocalRotation(Entity e, const glm::quat& rotation)
{
	instanceData.localRotation[e.id] = rotation;
	CalcTransform(e);
}

void TransformManager::SetLocalRotationEuler(Entity e, const glm::vec3& euler)
{
	instanceData.localRotation[e.id] = glm::quat(glm::vec3(glm::radians(euler.x), glm::radians(euler.y), glm::radians(euler.z)));
	CalcTransform(e);
}

void TransformManager::SetLocalScale(Entity e, const glm::vec3& scale)
{
	instanceData.localScale[e.id] = scale;
	CalcTransform(e);
}

void TransformManager::SetLocalToWorld(Entity e, const glm::mat4& localToWorld)
{
	Entity parent = instanceData.parent[e.id];
	glm::mat4 parentT = parent.IsValid() ? instanceData.localToWorld[parent.id] : glm::mat4(1.0f);

	glm::mat4 m = glm::inverse(parentT) * localToWorld;

	glm::quat r = glm::quat_cast(m);
	r = glm::normalize(r);

	instanceData.localPosition[e.id] = m[3];
	instanceData.localScale[e.id] = glm::vec3(glm::length(localToWorld[0]), glm::length(localToWorld[1]), glm::length(localToWorld[2]));	// Should it be m instead of local to world?
	instanceData.localRotation[e.id] = r;

	if (instanceData.modified[e.id] == false)
	{
		ModifiedTransform& mt = modifiedTransforms[numModifiedTransforms];
		mt.e = e;
		mt.localToWorld = &instanceData.localToWorld[e.id];
		numModifiedTransforms++;
	}

	instanceData.localToWorld[e.id] = localToWorld;
	instanceData.modified[e.id] = true;

	Entity child = instanceData.firstChild[e.id];
	while (child.IsValid())
	{
		CalcTransform(child, instanceData.localToWorld[e.id]);
		child = instanceData.nextSibling[child.id];
	}
}

void TransformManager::SetLocalToParent(Entity e, const glm::mat4& localToParent)
{
	glm::quat r = glm::quat_cast(localToParent);
	r = glm::normalize(r);

	instanceData.localPosition[e.id] = localToParent[3];
	instanceData.localScale[e.id] = glm::vec3(glm::length(localToParent[0]), glm::length(localToParent[1]), glm::length(localToParent[2]));
	instanceData.localRotation[e.id] = r;

	CalcTransform(e);
}

void TransformManager::Rotate(Entity e, const glm::vec3& rot)
{
	glm::quat q = glm::quat(glm::vec3(glm::radians(rot.x), glm::radians(rot.y), glm::radians(rot.z)));

	instanceData.localRotation[e.id] *= q;

	CalcTransform(e);
}

const glm::mat4& TransformManager::GetLocalToWorld(Entity e) const
{
	return instanceData.localToWorld[e.id];
}

bool TransformManager::HasParent(Entity e) const
{
	return instanceData.parent[e.id].IsValid();
}

bool TransformManager::HasChildren(Entity e) const
{
	return instanceData.firstChild[e.id].IsValid();
}

Entity TransformManager::GetParent(Entity e)
{
	return instanceData.parent[e.id];
}

Entity TransformManager::GetFirstChild(Entity e)
{
	return instanceData.firstChild[e.id];
}

Entity TransformManager::GetNextSibling(Entity e)
{
	return instanceData.nextSibling[e.id];
}

void TransformManager::CalcTransform(Entity e)
{
	Entity parent = instanceData.parent[e.id];
	glm::mat4 parentT = parent.IsValid() ? instanceData.localToWorld[parent.id] : glm::mat4(1.0f);

	CalcTransform(e, parentT);
}

void TransformManager::CalcTransform(Entity e, const glm::mat4& parent)
{
	glm::mat4 localToParent = glm::mat4_cast(instanceData.localRotation[e.id]);
	localToParent = glm::scale(localToParent, instanceData.localScale[e.id]);
	localToParent[3] = glm::vec4(instanceData.localPosition[e.id], 1.0f);

	if (instanceData.modified[e.id] == false)
	{
		ModifiedTransform& mt = modifiedTransforms[numModifiedTransforms];
		mt.e = e;
		mt.localToWorld = &instanceData.localToWorld[e.id];
		numModifiedTransforms++;
	}

	instanceData.localToWorld[e.id] = parent * localToParent;
	instanceData.modified[e.id] = true;

	Entity child = instanceData.firstChild[e.id];
	while (child.IsValid())
	{
		CalcTransform(child, instanceData.localToWorld[e.id]);
		child = instanceData.nextSibling[child.id];
	}
}

void TransformManager::RemoveTransform(Entity e)
{
	if (HasParent(e))
	{
		Entity parent = instanceData.parent[e.id];
		Entity prevSibling = instanceData.prevSibling[e.id];
		Entity nextSibling = instanceData.nextSibling[e.id];

		if (instanceData.firstChild[parent.id].id == e.id)
			instanceData.firstChild[parent.id] = nextSibling;
		else
		{
			assert(prevSibling.IsValid());

			// We're going to remove the transform between prev and next. Now the next of the prev is the next sibling
			instanceData.nextSibling[prevSibling.id] = nextSibling;
		}

		// If the transform had a next sibling now we also need to fix the prev sibling of the next sibling. The previous sibling will now be the previous sibling of the transform we removed.
		if (nextSibling.IsValid())
			instanceData.prevSibling[nextSibling.id] = prevSibling;
	}

	Entity child = instanceData.firstChild[e.id];
	while (child.IsValid())
	{
		RemoveParent(child);
		child = instanceData.nextSibling[child.id];
	}

	// Reset the slot where the entity is
	instanceData.localToWorld[e.id] = glm::mat4(1.0f);
	instanceData.localPosition[e.id] = glm::vec3(0.0f);
	instanceData.localRotation[e.id] = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	instanceData.localScale[e.id] = glm::vec3(1.0f);
	instanceData.parent[e.id] = { std::numeric_limits<unsigned int>::max() };
	instanceData.firstChild[e.id] = { std::numeric_limits<unsigned int>::max() };
	instanceData.prevSibling[e.id] = { std::numeric_limits<unsigned int>::max() };
	instanceData.nextSibling[e.id] = { std::numeric_limits<unsigned int>::max() };
}
