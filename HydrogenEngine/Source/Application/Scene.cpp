#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Application.hpp"
#include "Hydrogen/Camera.hpp"
#include "Hydrogen/Animation.hpp"

#include <string>

using namespace Hydrogen;

Entity::Entity(Scene* scene, std::string name)
	: m_Scene(scene)
{
	m_Entity = m_Scene->m_Registry.create();
	AddComponent<UUIDComponent>();
	AddComponent<TagComponent>(name);
	AddComponent<TransformComponent>(glm::mat4(1.0f));
	AddComponent<RelationshipComponent>(0);
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

MeshRendererComponent::MeshRendererComponent(Entity entity)
{
}

Scene::Scene()
	: m_PhysicsWorld(PhysicsWorld(this, { 0.0f, -9.81f, 0.0f })), m_ScriptSystem(this)
{
}

Entity Hydrogen::Scene::GetEntityByEntityID(uint32_t id)
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
	IterateComponents([&res, uuid](Hydrogen::Entity entity)
		{
			if (entity.GetUUID() == uuid)
			{
				res = entity;
			}
		});

	return res;
}

void Scene::CreateScripts()
{
	m_ScriptSystem.OnCreate();
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

json Scene::SerializeScene()
{
	json j;

	size_t i = 0;
	for (auto entity : m_Registry.view<entt::entity>())
	{
		json entityJson;

		if (m_Registry.all_of<TagComponent>(entity))
		{
			TagComponent::ToJson(entityJson["TagComponent"], m_Registry.get<TagComponent>(entity));
		}
		if (m_Registry.all_of<TransformComponent>(entity))
		{
			TransformComponent::ToJson(entityJson["TransformComponent"], m_Registry.get<TransformComponent>(entity));
		}
		if (m_Registry.all_of<RelationshipComponent>(entity))
		{
			RelationshipComponent::ToJson(entityJson["RelationshipComponent"], m_Registry.get<RelationshipComponent>(entity));
		}
		if (m_Registry.all_of<SkeletalMeshRendererComponent>(entity))
		{
			SkeletalMeshRendererComponent::ToJson(entityJson["SkeletalMeshRendererComponent"], m_Registry.get<SkeletalMeshRendererComponent>(entity));
		}
		if (m_Registry.all_of<AnimatorComponent>(entity))
		{
			AnimatorComponent::ToJson(entityJson["AnimatorComponent"], m_Registry.get<AnimatorComponent>(entity));
		}
		if (m_Registry.all_of<MeshRendererComponent>(entity))
		{
			MeshRendererComponent::ToJson(entityJson["MeshRendererComponent"], m_Registry.get<MeshRendererComponent>(entity));
		}
		if (m_Registry.all_of<RigidbodyComponent>(entity))
		{
			RigidbodyComponent::ToJson(entityJson["RigidbodyComponent"], m_Registry.get<RigidbodyComponent>(entity));
		}
		if (m_Registry.all_of<CameraComponent>(entity))
		{
			CameraComponent::ToJson(entityJson["CameraComponent"], m_Registry.get<CameraComponent>(entity));
		}
		if (m_Registry.all_of<ScriptsComponent>(entity))
		{
			ScriptsComponent::ToJson(entityJson["ScriptsComponent"], m_Registry.get<ScriptsComponent>(entity));
		}
		if (m_Registry.all_of<ColliderComponent>(entity))
		{
			ColliderComponent::ToJson(entityJson["ColliderComponent"], m_Registry.get<ColliderComponent>(entity));
		}
		if (m_Registry.all_of<DirectionalLightComponent>(entity))
		{
			DirectionalLightComponent::ToJson(entityJson["DirectionalLightComponent"], m_Registry.get<DirectionalLightComponent>(entity));
		}
		if (m_Registry.all_of<PointLightComponent>(entity))
		{
			PointLightComponent::ToJson(entityJson["PointLightComponent"], m_Registry.get<PointLightComponent>(entity));
		}

		j[std::to_string(m_Registry.get<UUIDComponent>(entity).UUID)] = entityJson;
	}

	return j;
}

void Scene::DeserializeScene(const json& j, AssetManager* assetManager)
{
	for (auto& [key, value] : j.items())
	{
		auto entity = m_Registry.create();

		Entity e;
		e.m_Entity = entity;
		e.m_Scene = this;

		e.AddComponent<UUIDComponent>(std::stoull(key));

		if (value.contains("TagComponent"))
		{
			TagComponent& component = m_Registry.emplace<TagComponent>(entity, e);
			TagComponent::FromJson(value["TagComponent"], component, assetManager);
		}
		if (value.contains("TransformComponent"))
		{
			TransformComponent& component = m_Registry.emplace<TransformComponent>(entity, e);
			TransformComponent::FromJson(value["TransformComponent"], component, assetManager);
		}
		if (value.contains("RelationshipComponent"))
		{
			RelationshipComponent& component = m_Registry.emplace<RelationshipComponent>(entity, e);
			RelationshipComponent::FromJson(value["RelationshipComponent"], component, assetManager);
		}
		else
		{
			e.AddComponent<RelationshipComponent>(0);
		}
		if (value.contains("SkeletalMeshRendererComponent"))
		{
			SkeletalMeshRendererComponent& component = m_Registry.emplace<SkeletalMeshRendererComponent>(entity, e);
			SkeletalMeshRendererComponent::FromJson(value["SkeletalMeshRendererComponent"], component, assetManager);
		}
		if (value.contains("AnimatorComponent"))
		{
			AnimatorComponent& component = m_Registry.emplace<AnimatorComponent>(entity, e);
			AnimatorComponent::FromJson(value["AnimatorComponent"], component, assetManager);
		}
		if (value.contains("MeshRendererComponent"))
		{
			MeshRendererComponent& component = m_Registry.emplace<MeshRendererComponent>(entity, e);
			MeshRendererComponent::FromJson(value["MeshRendererComponent"], component, assetManager);
		}
		if (value.contains("RigidbodyComponent"))
		{
			RigidbodyComponent& component = m_Registry.emplace<RigidbodyComponent>(entity, e);
			RigidbodyComponent::FromJson(value["RigidbodyComponent"], component, assetManager);
		}
		if (value.contains("CameraComponent"))
		{
			CameraComponent& component = m_Registry.emplace<CameraComponent>(entity, e);
			CameraComponent::FromJson(value["CameraComponent"], component, assetManager);
		}
		if (value.contains("ScriptsComponent"))
		{
			ScriptsComponent& component = m_Registry.emplace<ScriptsComponent>(entity, e);
			ScriptsComponent::FromJson(value["ScriptsComponent"], component, assetManager);
		}
		if (value.contains("ColliderComponent"))
		{
			ColliderComponent& component = m_Registry.emplace<ColliderComponent>(entity, e);
			ColliderComponent::FromJson(value["ColliderComponent"], component, assetManager);
		}
		if (value.contains("DirectionalLightComponent"))
		{
			DirectionalLightComponent& component = m_Registry.emplace<DirectionalLightComponent>(entity, e);
			DirectionalLightComponent::FromJson(value["DirectionalLightComponent"], component, assetManager);
		}
		if (value.contains("PointLightComponent"))
		{
			PointLightComponent& component = m_Registry.emplace<PointLightComponent>(entity, e);
			PointLightComponent::FromJson(value["PointLightComponent"], component, assetManager);
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
