#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <memory>

#include "Panel.hpp"

class SceneHierarchyPanel : public Panel
{
public:
    SceneHierarchyPanel();

    void SetContext(const std::shared_ptr<Hydrogen::Scene>& scene);
    Hydrogen::Entity GetSelectedEntity() const;

    void OnImGuiRender() override;
    const char* GetName() const override { return "Scene Hierarchy"; }

private:
    std::shared_ptr<Hydrogen::Scene> m_Scene;
    Hydrogen::Entity m_SelectedEntity;
};
