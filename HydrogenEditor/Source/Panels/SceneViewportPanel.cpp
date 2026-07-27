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

	Dockspace->AddToolBarCallback([this]() {
		if (ImGui::RadioButton("Translate", m_GuizmoTool == ImGuizmo::TRANSLATE)) m_GuizmoTool = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotate", m_GuizmoTool == ImGuizmo::ROTATE)) m_GuizmoTool = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale", m_GuizmoTool == ImGuizmo::SCALE)) m_GuizmoTool = ImGuizmo::SCALE;

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		ImGui::Checkbox("Show Gizmos", &m_ShowGizmos);
		ImGui::SameLine();
		ImGui::Checkbox("Show Colliders", &m_ShowColliders);
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
	Settings.Debug.RenderGrid = true;
	Settings.Display.Width = (uint64_t)m_ViewportSize.x;
	Settings.Display.Height = (uint64_t)m_ViewportSize.y;

	CollectGizmos(Settings.Debug.Gizmos);

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
			m_FreeCam.ViewportWidth = static_cast<uint32_t>(m_ViewportSize.x);
			m_FreeCam.ViewportHeight = static_cast<uint32_t>(m_ViewportSize.y);
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

void SceneViewportPanel::CollectGizmos(std::vector<Gizmo>& gizmos)
{
	if (m_ShowGizmos)
	{
		m_Scene->IterateComponents([&gizmos](Entity entity)
			{
				if (entity.HasComponent<CameraComponent>())
				{
					gizmos.push_back({
						Gizmo::Type::Billboard,
						Application::Get()->MainAssetManager.GetAsset<TextureAsset>("camera.png"),
						entity.GetComponent<TransformComponent>().GetPosition(),
						{1, 1}
						});
				}
				else if (entity.HasComponent<PointLightComponent>())
				{
					gizmos.push_back({
						Gizmo::Type::Billboard,
						Application::Get()->MainAssetManager.GetAsset<TextureAsset>("point_light.png"),
						entity.GetComponent<TransformComponent>().GetPosition(),
						{1, 1}
						});
				}
				else if (entity.HasComponent<DirectionalLightComponent>())
				{
					gizmos.push_back({
						Gizmo::Type::Billboard,
						Application::Get()->MainAssetManager.GetAsset<TextureAsset>("directional_light.png"),
						entity.GetComponent<TransformComponent>().GetPosition(),
						{1, 1}
						});
				}
			});
	}

	if (m_ShowColliders)
	{
		m_Scene->IterateComponents<TransformComponent, ColliderComponent>([&gizmos](Entity entity, TransformComponent& transform, ColliderComponent& collider)
			{
				glm::vec3 translation, rotation, scale;
				TransformComponent::DecomposeTransform(transform.Transform, translation, rotation, scale);

				glm::quat worldRot = glm::quat(rotation);
				glm::vec3 colliderWorldPos = translation + worldRot * collider.LocalPosition;
				glm::quat colliderWorldRot = worldRot * collider.LocalRotation;

				if (collider.ColliderType == ColliderComponent::Type::Box)
				{
					gizmos.push_back({
						Gizmo::Type::WireframeBox,
						nullptr,
						colliderWorldPos,
						{1.0f, 1.0f},
						glm::vec3(0.0f, 1.0f, 0.0f),
						colliderWorldRot,
						collider.Size * scale
						});
				}
				else if (collider.ColliderType == ColliderComponent::Type::Sphere)
				{
					float maxScale = glm::max(glm::max(scale.x, scale.y), scale.z);
					gizmos.push_back({
						Gizmo::Type::WireframeSphere,
						nullptr,
						colliderWorldPos,
						{1.0f, 1.0f},
						glm::vec3(0.0f, 1.0f, 0.0f),
						colliderWorldRot,
						glm::vec3(1.0f),
						collider.Radius * maxScale
						});
				}
				else if (collider.ColliderType == ColliderComponent::Type::Capsule)
				{
					float maxScale = glm::max(glm::max(scale.x, scale.y), scale.z);
					glm::vec3 capsuleScale = glm::vec3(
						collider.Radius * maxScale,
						(collider.Height + collider.Radius * 2.0f) * maxScale,
						collider.Radius * maxScale
					);

					gizmos.push_back({
						Gizmo::Type::WireframeCapsule,
						nullptr,
						colliderWorldPos,
						{1.0f, 1.0f},
						glm::vec3(0.0f, 0.7f, 1.0f),
						colliderWorldRot,
						capsuleScale
						});
				}
			});
	}
}
