#include "Panels/GameViewportPanel.hpp"

using namespace Hydrogen;

extern VkSampler ImguiSampler;
extern ImGuiTextureCache TextureCache;

void GameViewportPanel::OnAttach()
{
	Dockspace->GetEventBus().Subscribe<SceneChangeEvent>([this](const SceneChangeEvent& e) {
		m_Scene = e.Scene;
		});

	m_Renderer = std::make_unique<Renderer>(Application::Get()->GetRenderDevice());
}

void GameViewportPanel::OnUpdate(float dt)
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

	Entity cameraEntity;
	if (m_IsVisible && m_ViewportSize.x != 0 && m_ViewportSize.y != 0 && GetAndUpdateCamera(cameraEntity))
	{
		const auto& camera = cameraEntity.GetComponent<CameraComponent>();
		const auto& cameraPos = cameraEntity.GetComponent<TransformComponent>().GetPosition();

		m_RenderedScene = DefaultRenderer::RenderSceneDeferred(
			m_Renderer.get(), Settings, camera, cameraPos, m_Scene
		).ImageView;
	}
}

void GameViewportPanel::OnImGuiRender()
{
	ImVec2 contentRegion = ImGui::GetContentRegionAvail();

	m_IsVisible = ImGui::Begin(GetID().c_str(), &m_IsOpen);
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
		}
	}

	ImGui::End();
}

void GameViewportPanel::UpdateCameraViewportSize(Hydrogen::CameraComponent& camera, const glm::ivec2& size)
{
	if (size.x > 0 && size.y > 0 && (camera.ViewportWidth != size.x || camera.ViewportHeight != size.y))
	{
		camera.ViewportWidth = size.x;
		camera.ViewportHeight = size.y;
		camera.CalculateProj();
	}
}

bool GameViewportPanel::GetAndUpdateCamera(Entity& outEntity)
{
	Entity activeCameraEntity;
	m_Scene->IterateComponents<CameraComponent>([&](Entity entity, CameraComponent& camera) {
		if (camera.Active) activeCameraEntity = entity;
		});

	if (activeCameraEntity.IsValid())
	{
		auto& camera = activeCameraEntity.GetComponent<CameraComponent>();
		camera.CalculateView(activeCameraEntity);
		UpdateCameraViewportSize(camera, { (int)m_ViewportSize.x, (int)m_ViewportSize.y });
		outEntity = activeCameraEntity;
		return true;
	}
	return false;
}
