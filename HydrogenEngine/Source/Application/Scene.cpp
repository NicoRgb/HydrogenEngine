#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Application.hpp"
#include "Hydrogen/Camera.hpp"
#include "Hydrogen/ScriptEngine.hpp"

#include <string>

using namespace Hydrogen;

void ScriptSystem::OnCreate()
{
	m_Scene->IterateComponents<ScriptComponent>([&](Entity entity, ScriptComponent& script)
		{
			script.Environment = sol::environment(ScriptEngine::GetLuaState(), sol::create, ScriptEngine::GetLuaState().globals());

			script.Chunk = ScriptEngine::GetLuaState().load(script.Script->GetContent());
			if (!script.Chunk.valid())
			{
				sol::error err = script.Chunk;
				HY_ENGINE_ERROR("[LUA LOAD ERROR]: {}", err.what());
			}
			else
			{
				sol::protected_function_result result = script.Chunk();

				if (!result.valid())
				{
					sol::error err = script.Chunk;
					HY_ENGINE_ERROR("[LUA RUNTIME ERROR]: {}", err.what());
				}
			}

			sol::protected_function on_create = script.Environment["on_create"];
			if (on_create.valid())
			{
				sol::protected_function_result r = on_create(entity);
				if (!r.valid())
				{
					sol::error err = r;
					HY_ENGINE_ERROR("[LUA RUNTIME ERROR]: {}", err.what());
				}
			}
		});
}

void ScriptSystem::OnUpdate(float dt)
{
	m_Scene->IterateComponents<ScriptComponent>([&](Entity entity, ScriptComponent& script)
		{
			sol::protected_function on_update = script.Environment["on_update"];
			if (on_update.valid())
			{
				sol::protected_function_result r = on_update(entity, dt);
				if (!r.valid())
				{
					sol::error err = r;
					HY_ENGINE_ERROR("[LUA RUNTIME ERROR]: {}", err.what());
				}
			}
		});
}

Entity::Entity(const std::shared_ptr<Scene>& scene, std::string name)
	: m_Scene(scene)
{
	m_Entity = m_Scene->m_Registry.create();
	AddComponent<UUIDComponent>();
	AddComponent<TagComponent>(name);
	AddComponent<TransformComponent>(glm::mat4(1.0f));
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

void MeshRendererComponent::OnImGuiRender(MeshRendererComponent& t)
{
	if (ImGui::TreeNode("Mesh Renderer"))
	{
		if (t.Mesh)
		{
			ImGui::Text(t.Mesh->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<MeshAsset>(newPath.filename().string());
				if (asset)
				{
					t.Mesh = asset;
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (t.Texture)
		{
			ImGui::Text(t.Texture->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(newPath.filename().string());
				if (asset)
				{
					t.Texture = asset;
				}
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::TreePop();
	}
}

Scene::Scene()
	: m_PhysicsWorld(PhysicsWorld(this, { 0.0f, -9.81f, 0.0f })), m_ScriptSystem(this)
{
}

void Scene::CreateScripts()
{
	m_ScriptSystem.OnCreate();
}

void Scene::UpdatePhysics(float timestep)
{
	m_PhysicsWorld.Update(timestep);
}

void Scene::UpdateScripts(float dt)
{
	m_ScriptSystem.OnUpdate(dt);
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
		if (m_Registry.all_of<ScriptComponent>(entity))
		{
			ScriptComponent::ToJson(entityJson["ScriptComponent"], m_Registry.get<ScriptComponent>(entity));
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
		e.m_Scene = shared_from_this();

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
		if (value.contains("ScriptComponent"))
		{
			ScriptComponent& component = m_Registry.emplace<ScriptComponent>(entity, e);
			ScriptComponent::FromJson(value["ScriptComponent"], component, assetManager);
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

void ScriptComponent::OnImGuiRender(ScriptComponent& s)
{
	if (ImGui::TreeNode("Script"))
	{
		if (s.Script)
		{
			ImGui::Text(s.Script->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<ScriptAsset>(newPath.filename().string());
				if (asset)
				{
					s.Script = asset;
				}
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::TreePop();
	}
}
