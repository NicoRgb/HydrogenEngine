#include <imgui.h>
#include "InspectorPanel.hpp"

using namespace Hydrogen;

template<typename T>
const char* GetComponentName()
{
	if constexpr (std::is_same_v<T, MeshRendererComponent>) return "Mesh Renderer";
	else if constexpr (std::is_same_v<T, DirectionalLightComponent>) return "Directional Light";
	else if constexpr (std::is_same_v<T, PointLightComponent>) return "Point Light";
	else if constexpr (std::is_same_v<T, RigidbodyComponent>) return "Rigidbody";
	else if constexpr (std::is_same_v<T, ColliderComponent>) return "Collider";
	else if constexpr (std::is_same_v<T, CameraComponent>) return "Camera";
	else if constexpr (std::is_same_v<T, ScriptComponent>) return "Script";
	else return "Unknown Component";
}

template<typename T>
static void DrawComponent(Entity entity)
{
	if (!entity.HasComponent<T>())
	{
		return;
	}

	if constexpr (!std::is_same_v<T, TagComponent> && !std::is_same_v<T, TransformComponent>)
	{
		std::stringstream sstream;
		sstream << "X###Remove_";
		sstream << GetComponentName<T>();

		if (ImGui::Button(sstream.str().c_str()))
		{
			entity.RemoveComponent<T>();
			return;
		}

		ImGui::SameLine();
	}
	
	T& component = entity.GetComponent<T>();
	T::OnImGuiRender(component);
}

template<typename... Ts>
static void DrawAllComponents(Entity entity)
{
	(DrawComponent<Ts>(entity), ...);
}

template<typename... Ts>
void DrawAddComponentMenu(Scene* scene, Entity entity)
{
	if (ImGui::Button("Add Component"))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		(..., [&] {
			using T = Ts;
			if (!entity.HasComponent<T>())
			{
				if (ImGui::MenuItem(GetComponentName<T>()))
				{
					entity.AddComponent<T>();
					ImGui::CloseCurrentPopup();
				}
			}
			}());

		ImGui::EndPopup();
	}
}

void InspectorPanel::SetContext(Scene* scene, Entity selected)
{
	m_Scene = scene;
	m_SelectedEntity = selected;
}

void InspectorPanel::OnImGuiRender()
{
	ImGui::Begin(GetName());
	if (m_SelectedEntity.IsValid())
	{
		if (ImGui::Button("Remove"))
		{
			auto e = m_SelectedEntity;
			m_SelectedEntity = Entity();
			e.Delete();
			ImGui::End();
			return;
		}

		ImGui::Separator();
		DrawAllComponents<TagComponent, TransformComponent, MeshRendererComponent, DirectionalLightComponent, PointLightComponent, RigidbodyComponent, ColliderComponent, CameraComponent, ScriptComponent>
			(m_SelectedEntity);
		ImGui::Separator();
		DrawAddComponentMenu<MeshRendererComponent, DirectionalLightComponent, PointLightComponent, RigidbodyComponent, ColliderComponent, CameraComponent, ScriptComponent>
			(m_Scene, m_SelectedEntity);
	}
	else
	{
		ImGui::Text("No entity selected");
	}
	ImGui::End();
}
