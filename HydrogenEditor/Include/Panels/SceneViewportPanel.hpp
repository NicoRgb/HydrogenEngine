#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>

#include "GUISystem.hpp"
#include "Panels/GraphicsSettingsPanel.hpp"

class SceneViewportPanel : public EditorPanel
{
public:
	virtual std::string GetTitle() const override { static std::string t = "Scene"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Center; }

	virtual void OnAttach() override;
	virtual void OnUpdate(float dt) override;

	virtual void OnImGuiRender() override;

private:
	void DrawGizmo();
	void CollectGizmos(std::vector<Hydrogen::Gizmo>& gizmos);

	bool m_IsVisible = false;
	bool m_IsHovered = false;
	glm::vec2 m_ViewportSize{ 1920.0f, 1080.0f };
	glm::vec2 m_ViewportBounds[2];
	VkImageView m_RenderedScene = VK_NULL_HANDLE;

	std::unique_ptr<Hydrogen::Renderer> m_Renderer;
	Hydrogen::FreeCamera m_FreeCam;

	Hydrogen::Scene* m_Scene;
	Hydrogen::Entity m_SelectedEntity;
	ImGuizmo::OPERATION m_GuizmoTool;
};
