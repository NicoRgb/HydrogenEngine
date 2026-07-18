#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <memory>
#include <tuple>

#include "Panel.hpp"

class InspectorPanel : public Panel
{
public:
	void SetContext(Hydrogen::Scene* scene, Hydrogen::Entity selected);
	void OnImGuiRender() override;
	const char* GetName() const override { return "Inspector"; }

	void SetSelectedEntity(Hydrogen::Entity entity) { m_SelectedEntity = entity; }

private:
	Hydrogen::Scene* m_Scene;
	Hydrogen::Entity m_SelectedEntity;
};
