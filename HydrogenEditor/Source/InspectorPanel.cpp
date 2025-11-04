#include <imgui.h>
#include "InspectorPanel.hpp"

template<typename T>
static void DrawComponent(Hydrogen::Entity entity)
{
    T& component = entity.GetComponent<T>();
    T::OnImGuiRender(component);
}

using ComponentTypes = std::tuple<Hydrogen::TagComponent, Hydrogen::TransformComponent, Hydrogen::MeshRendererComponent>;

template<typename... Ts>
static void DrawAllComponents(Hydrogen::Entity entity, std::tuple<Ts...>)
{
    (DrawComponent<Ts>(entity), ...);
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
        DrawAllComponents(m_SelectedEntity, ComponentTypes{});
    else
        ImGui::Text("No entity selected");
    ImGui::End();
}
