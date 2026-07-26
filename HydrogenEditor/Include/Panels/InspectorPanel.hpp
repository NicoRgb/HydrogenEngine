#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <memory>
#include <tuple>

#include "GUISystem.hpp"

class InspectorPanel : public EditorPanel
{
public:
	virtual void OnAttach();
	virtual void OnDetach() {}

	virtual void OnUpdate(float deltaTime) {}
	virtual void OnImGuiRender();

	virtual std::string GetTitle() const { return "Inspector"; }

	virtual DockDirection GetDefaultDockDirection() const { return DockDirection::Left_Bottom; }
	virtual float GetDefaultDockSplitRatio() const { return 0.25f; }

private:
	Hydrogen::Scene* m_Scene;
	Hydrogen::Entity m_SelectedEntity;
};
