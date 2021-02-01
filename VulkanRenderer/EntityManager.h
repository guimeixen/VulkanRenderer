#pragma once

#include <limits>
#include <functional>
#include <vector>
#include <stack>

struct Entity
{
	unsigned int id;

	bool IsValid() const { return id != std::numeric_limits<unsigned int>::max(); }
};

class EntityManager
{
public:
	EntityManager();

	Entity Create();
	Entity Duplicate(Entity e);
	void SetEnabled(Entity e, bool enable);
	void Destroy(Entity e);
	bool IsEntityEnabled(Entity e);

	void AddComponentDestroyCallback(const std::function<void(Entity)>& callback);
	void AddComponentDuplicateCallback(const std::function<void(Entity, Entity)>& callback);
	void AddComponentSetEnabledCallback(const std::function<void(Entity, bool)>& callback);

private:
	Entity nextEntity;
	std::vector<std::function<void(Entity)>> destroyCallbacks;
	std::vector<std::function<void(Entity, Entity)>> duplicateEntityCallbacks;
	std::vector<std::function<void(Entity, bool)>> setEnabledEntityCallbacks;
	std::stack<unsigned int> freeIndices;
	std::vector<unsigned int> disabledEntities;
};

