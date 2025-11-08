#include "SceneHierarchyPanel.hpp"
#include <imgui.h>

SceneHierarchyPanel::SceneHierarchyPanel() : m_SelectedEntity() {}

void SceneHierarchyPanel::SetContext(const std::shared_ptr<Hydrogen::Scene>& scene)
{
    m_Scene = scene;
}

Hydrogen::Entity SceneHierarchyPanel::GetSelectedEntity() const
{
    return m_SelectedEntity;
}

void SceneHierarchyPanel::OnImGuiRender()
{
    ImGui::Begin(GetName());

    if (m_Scene)
    {
        if (ImGui::Button("Add Entity"))
        {
            Hydrogen::Entity(m_Scene, "New Entity");
        }

        ImGui::Separator();

        m_Scene->IterateComponents<Hydrogen::TagComponent>([&](Hydrogen::Entity entity, const auto& tag)
            {
            bool selected = (m_SelectedEntity == entity);
            if (ImGui::Selectable(tag.Name.c_str(), selected))
                m_SelectedEntity = entity;
            });
    }
    else
    {
        m_SelectedEntity = Hydrogen::Entity();
    }

    ImGui::End();
}
