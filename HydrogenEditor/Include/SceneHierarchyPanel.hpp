#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <memory>

#include "Panel.hpp"

class SceneHierarchyPanel : public Panel
{
public:
    SceneHierarchyPanel();

    void SetContext(Hydrogen::Scene* scene);
    Hydrogen::Entity GetSelectedEntity() const;

    void OnImGuiRender() override;
    const char* GetName() const override { return "Scene Hierarchy"; }

	void SetSelectedEntity(uint64_t uuid) { m_SelectedEntityUUID = uuid; }

private:
    Hydrogen::Scene* m_Scene;
    uint64_t m_SelectedEntityUUID;
};
