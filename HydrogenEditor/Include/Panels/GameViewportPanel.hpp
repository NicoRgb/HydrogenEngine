#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>

#include "GUISystem.hpp"
#include "Panels/GraphicsSettingsPanel.hpp"

class GameViewportPanel : public EditorPanel
{
public:
	virtual std::string GetTitle() const override { static std::string t = "Game"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Center; }

	virtual void OnAttach() override;
	virtual void OnUpdate(float dt) override;

	virtual void OnImGuiRender() override;

private:
	void UpdateCameraViewportSize(Hydrogen::CameraComponent& camera, const glm::ivec2& size);
	bool GetAndUpdateCamera(Hydrogen::Entity& outEntity);

	bool m_IsVisible = false;
	glm::vec2 m_ViewportSize{ 1920.0f, 1080.0f };
	glm::vec2 m_ViewportBounds[2];
	VkImageView m_RenderedScene = VK_NULL_HANDLE;

	std::unique_ptr<Hydrogen::Renderer> m_Renderer;
	Hydrogen::Scene* m_Scene;
};
