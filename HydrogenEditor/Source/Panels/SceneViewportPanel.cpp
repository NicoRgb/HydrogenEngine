#include "Panels/SceneViewportPanel.hpp"

using namespace Hydrogen;

extern VkSampler ImguiSampler;
extern ImGuiTextureCache TextureCache;

void SceneViewportPanel::OnAttach()
{
	m_GuizmoTool = ImGuizmo::TRANSLATE;

	Dockspace->GetEventBus().Subscribe<SceneChangeEvent>([this](const SceneChangeEvent& e) {
		m_Scene = e.Scene;
		});

	Dockspace->GetEventBus().Subscribe<EntitySelectedEvent>([this](const EntitySelectedEvent& e) {
		m_SelectedEntity = e.SelectedEntity;
		});

	Dockspace->GetEventBus().Subscribe<ToolChangeEvent>([this](const ToolChangeEvent& e) {
		m_GuizmoTool = e.GuizmoTool;
		});

	m_FreeCam.ViewportWidth = 512;
	m_FreeCam.ViewportHeight = 512;
	m_FreeCam.SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
	m_FreeCam.CalculateProj();
	m_FreeCam.Active = true;

	m_Renderer = std::make_unique<Renderer>(Application::Get()->GetRenderDevice());
}

void SceneViewportPanel::OnUpdate(float dt)
{
	m_RenderedScene = VK_NULL_HANDLE;

	const auto* SettingsPanel = Dockspace->TryGetPanel<GraphicsSettingsPanel>();
	RenderSettings Settings = {};
	if (SettingsPanel)
	{
		Settings = SettingsPanel->GetSettings();
	}

	Settings.Display.RenderToSwapChain = false;
	Settings.Display.Width = (uint64_t)m_ViewportSize.x;
	Settings.Display.Height = (uint64_t)m_ViewportSize.y;

	if (m_IsVisible && m_ViewportSize.x != 0 && m_ViewportSize.y != 0)
	{
		if (Input::IsMouseButtonDown(KeyCode::MouseRight) && m_IsHovered)
		{
			Viewport::ConfineCursor(
				m_ViewportBounds[0].x, m_ViewportBounds[1].x,
				m_ViewportBounds[0].y, m_ViewportBounds[1].y
			);
			m_FreeCam.Update(dt);
		}
		else
		{
			Viewport::ReleaseCursor();
		}

		m_FreeCam.CalculateView();

		if (m_ViewportSize.x > 0 && m_ViewportSize.y > 0 && (m_FreeCam.ViewportWidth != m_ViewportSize.x || m_FreeCam.ViewportHeight != m_ViewportSize.y))
		{
			m_FreeCam.ViewportWidth = m_ViewportSize.x;
			m_FreeCam.ViewportHeight = m_ViewportSize.y;
			m_FreeCam.CalculateProj();
		}

		m_RenderedScene = DefaultRenderer::RenderSceneDeferred(
			m_Renderer.get(), Settings, m_FreeCam, m_FreeCam.GetPosition(), m_Scene
		).ImageView;
	}
}

void SceneViewportPanel::OnImGuiRender()
{
	ImVec2 contentRegion = ImGui::GetContentRegionAvail();

	m_IsVisible = ImGui::Begin(GetID().c_str(), &m_IsOpen);
	m_IsHovered = ImGui::IsWindowHovered();
	m_ViewportSize = { contentRegion.x, contentRegion.y };

	ImVec2 viewportMinRegion = ImGui::GetWindowContentRegionMin();
	ImVec2 viewportMaxRegion = ImGui::GetWindowContentRegionMax();
	ImVec2 viewportOffset = ImGui::GetWindowPos();

	glm::vec2 BoundsMin = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
	glm::vec2 BoundsMax = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

	m_ViewportBounds[0] = { BoundsMin.x, BoundsMin.y };
	m_ViewportBounds[1] = { BoundsMax.x, BoundsMax.y };

	if (m_IsVisible)
	{
		if (m_RenderedScene != VK_NULL_HANDLE && ImguiSampler != VK_NULL_HANDLE)
		{
			ImTextureID textureID = TextureCache.GetTextureID(m_RenderedScene, ImguiSampler);
			ImGui::Image(textureID, contentRegion);
			DrawGizmo();
		}
	}

	ImGui::End();
}

void SceneViewportPanel::DrawGizmo()
{
	if (!m_SelectedEntity.IsValid()) return;

	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y,
		m_ViewportBounds[1].x - m_ViewportBounds[0].x,
		m_ViewportBounds[1].y - m_ViewportBounds[0].y);

	auto& tc = m_SelectedEntity.GetComponent<TransformComponent>();
	glm::mat4 transform = tc.Transform;
	glm::mat4 view = m_FreeCam.View;
	glm::mat4 proj = m_FreeCam.Proj;
	proj[1][1] *= -1;

	ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
		m_GuizmoTool, ImGuizmo::WORLD, glm::value_ptr(transform));

	if (ImGuizmo::IsUsing())
		tc.Transform = transform;
}
