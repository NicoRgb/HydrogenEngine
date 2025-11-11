#include "SceneHierarchyPanel.hpp"
#include <imgui.h>

SceneHierarchyPanel::SceneHierarchyPanel() : m_SelectedEntityUUID(0) {}

void SceneHierarchyPanel::SetContext(const std::shared_ptr<Hydrogen::Scene>& scene)
{
    m_Scene = scene;
}

Hydrogen::Entity SceneHierarchyPanel::GetSelectedEntity() const
{
    Hydrogen::Entity selectedEntity;
    m_Scene->IterateComponents<Hydrogen::UUIDComponent>([&](Hydrogen::Entity entity, const Hydrogen::UUIDComponent& uuid)
        {
            if (uuid.UUID == m_SelectedEntityUUID)
            {
                selectedEntity = entity;
            }
        });

    return selectedEntity;
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
                bool selected = (m_SelectedEntityUUID == entity.GetUUID());
                if (ImGui::Selectable(tag.Name.c_str(), selected))
                    m_SelectedEntityUUID = entity.GetUUID();
            });
    }
    else
    {
        m_SelectedEntityUUID = Hydrogen::Entity().GetUUID();
    }

    ImGui::End();
}
