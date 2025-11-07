#include "Hydrogen/Scene.hpp"
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
