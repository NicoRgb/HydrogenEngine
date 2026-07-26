#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <memory>

#include "GUISystem.hpp"

class SceneHierarchyPanel : public EditorPanel
{
public:
	virtual void OnAttach();
	virtual void OnDetach() {}

	virtual void OnUpdate(float deltaTime) {}
	virtual void OnImGuiRender();

	virtual std::string GetTitle() const { return "Scene Hierarchy"; }

	virtual DockDirection GetDefaultDockDirection() const { return DockDirection::Left_Top; }
	virtual float GetDefaultDockSplitRatio() const { return 0.25f; }

private:
	Hydrogen::Entity GetSelectedEntity() const;

    Hydrogen::Scene* m_Scene;
    uint64_t m_SelectedEntityUUID;
};
