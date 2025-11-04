#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <memory>
#include <tuple>

#include "Panel.hpp"

class InspectorPanel : public Panel
{
public:
    void SetContext(const std::shared_ptr<Hydrogen::Scene>& scene, Hydrogen::Entity selected);
    void OnImGuiRender() override;
    const char* GetName() const override { return "Inspector"; }

private:
    std::shared_ptr<Hydrogen::Scene> m_Scene;
    Hydrogen::Entity m_SelectedEntity;
};
