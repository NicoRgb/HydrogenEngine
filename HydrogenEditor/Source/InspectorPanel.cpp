#include <imgui.h>
#include "InspectorPanel.hpp"

template<typename T>
static void DrawComponent(Hydrogen::Entity entity)
{
    if (!entity.HasComponent<T>())
    {
        return;
    }

    T& component = entity.GetComponent<T>();
    T::OnImGuiRender(component);
}

using ComponentTypes = std::tuple<Hydrogen::TagComponent, Hydrogen::TransformComponent, Hydrogen::MeshRendererComponent, Hydrogen::RigidbodyComponent, Hydrogen::CameraComponent>;
using AddComponentTypes = std::tuple<Hydrogen::MeshRendererComponent, Hydrogen::RigidbodyComponent, Hydrogen::CameraComponent>;

template<typename T>
const char* GetComponentName()
{
    if constexpr (std::is_same_v<T, Hydrogen::MeshRendererComponent>) return "Mesh Renderer";
    else if constexpr (std::is_same_v<T, Hydrogen::RigidbodyComponent>) return "Rigidbody";
    else if constexpr (std::is_same_v<T, Hydrogen::CameraComponent>) return "Camera";
    else return "Unknown Component";
}

template<typename... Ts>
static void DrawAllComponents(Hydrogen::Entity entity, std::tuple<Ts...>)
{
    (DrawComponent<Ts>(entity), ...);
}

template<typename... Ts>
void DrawAddComponentMenu(const std::shared_ptr<Hydrogen::Scene>& scene, Hydrogen::Entity entity, std::tuple<Ts...>)
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

void InspectorPanel::SetContext(const std::shared_ptr<Hydrogen::Scene>& scene, Hydrogen::Entity selected)
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
            m_SelectedEntity = Hydrogen::Entity();
            e.Delete();
            return;
        }

        ImGui::Separator();
        DrawAllComponents(m_SelectedEntity, ComponentTypes{ m_SelectedEntity, m_SelectedEntity, m_SelectedEntity, m_SelectedEntity, m_SelectedEntity });
        ImGui::Separator();
        DrawAddComponentMenu(m_Scene, m_SelectedEntity, AddComponentTypes{ m_SelectedEntity, m_SelectedEntity, m_SelectedEntity });
    }
    else
    {
        ImGui::Text("No entity selected");
    }
    ImGui::End();
}
