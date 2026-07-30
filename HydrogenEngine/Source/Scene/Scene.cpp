#include "Hydrogen/Application.hpp"
#include "Hydrogen/Scene/Camera.hpp"
#include "Hydrogen/Scene/Scene.hpp"
#include "Hydrogen/Scene/Animation.hpp"
#include "Hydrogen/Scene/Components.hpp"

#include <string>

using namespace Hydrogen;

Entity::Entity(Scene* scene, std::string name)
	: m_Scene(scene)
{
	m_Entity = m_Scene->m_Registry.create();
	AddComponent<UUIDComponent>();
	AddComponent<TagComponent>();
	GetComponent<TagComponent>().Name = name;

	AddComponent<TransformComponent>();
	AddComponent<RelationshipComponent>();
}

Entity::Entity(entt::entity id, Scene* scene)
	: m_Scene(scene), m_Entity(id)
{
}

Entity::Entity()
	: m_Scene(nullptr), m_Entity(entt::null)
{
}

uint64_t Entity::GetUUID()
{
	return GetComponent<UUIDComponent>().UUID;
}

void Entity::SetUUID(uint64_t uuid)
{
	GetComponent<UUIDComponent>().UUID = uuid;
}

void Entity::Delete()
{
	m_Scene->m_Registry.destroy(m_Entity);
}
Scene::Scene()
	: m_PhysicsWorld(PhysicsWorld(this, { 0.0f, -9.81f, 0.0f })), m_ScriptSystem(this)
{
}

Entity Scene::GetEntityByEntityID(uint32_t id)
{
	Entity e;
	e.m_Entity = entt::entity(id);
	e.m_Scene = this;

	if (e.IsValid())
		return e;
	else
		return Entity();
}

Entity Scene::GetEntityByUUID(uint64_t uuid)
{
	Entity res;
	IterateComponents([&res, uuid](Entity entity)
		{
			if (entity.GetUUID() == uuid)
			{
				res = entity;
			}
		});

	return res;
}

void Scene::UpdatePhysics(float timestep)
{
	m_PhysicsWorld.UpdatePhysics(timestep);
}

void Scene::Update(float dt)
{
	m_PhysicsWorld.SyncTransforms();
	m_ScriptSystem.OnUpdate(dt);

	IterateComponents<AnimatorComponent>([dt](Entity e, AnimatorComponent& anim)
		{
			anim.UpdateAnimation(dt);
		});
}

static void SanitizeJsonFloats(json& j, float precision = 10000.0f)
{
	if (j.is_object())
	{
		for (auto it = j.begin(); it != j.end(); ++it)
		{
			SanitizeJsonFloats(it.value(), precision);
		}
	}
	else if (j.is_array())
	{
		for (auto& val : j)
		{
			SanitizeJsonFloats(val, precision);
		}
	}
	else if (j.is_number_float())
	{
		double val = j.get<double>();
		j = std::round(val * precision) / precision;
	}
}

json Scene::SerializeScene()
{
	json root;
	for (auto entityID : m_Registry.view<entt::entity>())
	{
		Entity e;
		e.m_Entity = entityID;
		e.m_Scene = this;

		std::string entityKey = std::to_string(e.GetComponent<UUIDComponent>().UUID);

		json entityJson;
		for (const auto& [name, handlers] : ComponentRegistry::Get().GetAllHandlers())
		{
			json componentJson;
			handlers.Serialize(componentJson, e);
			if (!componentJson.empty())
				entityJson[name] = componentJson;
		}

		root[entityKey] = entityJson;
	}

	SanitizeJsonFloats(root);

	return root;
}

void Scene::DeserializeScene(const json& j)
{
	for (auto& [key, value] : j.items())
	{
		auto entity = m_Registry.create();

		Entity e;
		e.m_Entity = entity;
		e.m_Scene = this;

		e.AddComponent<UUIDComponent>(std::stoull(key));

		for (const auto& [name, handlers] : ComponentRegistry::Get().GetAllHandlers())
		{
			if (value.contains(name))
				handlers.Deserialize(value[name], e);
		}
	}
}

UUIDComponent::UUIDComponent(Entity)
{
	UUID = GenerateUUID();
}

UUIDComponent::UUIDComponent(Entity, uint64_t uuid)
{
	UUID = uuid;
}
