#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Physics.hpp"
#include "Hydrogen/Application.hpp"

using namespace Hydrogen;

Entity::Entity(const std::shared_ptr<Scene>& scene, std::string name)
	: m_Scene(scene)
{
	m_Entity = m_Scene->m_Registry.create();
	AddComponent<TagComponent>(name);
	AddComponent<TransformComponent>(glm::mat4(1.0f));
}

Entity::Entity()
	: m_Scene(nullptr), m_Entity(entt::null)
{
}

void Entity::Delete()
{
	m_Scene->m_Registry.destroy(m_Entity);
}

void MeshRendererComponent::OnImGuiRender(MeshRendererComponent& t)
{
	if (ImGui::TreeNode("Mesh Renderer"))
	{
		ImGui::Text(t.Mesh->GetPath().c_str());
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

		ImGui::Text(t.Texture->GetPath().c_str());
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

		j[std::to_string(i++)] = entityJson;
	}

	return j;
}

void Scene::DeserializeScene(const json& j, AssetManager* assetManager)
{
	for (auto& [key, value] : j.items())
	{
		auto entity = m_Registry.create();

		if (value.contains("TagComponent"))
		{
			TagComponent& component = m_Registry.emplace<TagComponent>(entity);
			TagComponent::FromJson(value["TagComponent"], component, assetManager);
		}
		if (value.contains("TransformComponent"))
		{
			TransformComponent& component = m_Registry.emplace<TransformComponent>(entity);
			TransformComponent::FromJson(value["TransformComponent"], component, assetManager);
		}
		if (value.contains("MeshRendererComponent"))
		{
			MeshRendererComponent& component = m_Registry.emplace<MeshRendererComponent>(entity);
			MeshRendererComponent::FromJson(value["MeshRendererComponent"], component, assetManager);
		}
		//if (value.contains("RigidbodyComponent"))
		//{
		//	RigidbodyComponent& component = m_Registry.emplace<RigidbodyComponent>(entity);
		//	RigidbodyComponent::FromJson(value["RigidbodyComponent"], component, assetManager);
		//}
	}
}
