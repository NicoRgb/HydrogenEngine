#include <Hydrogen/HydrogenMain.hpp>

#include "AssetBrowserPanel.hpp"
#include "AssetInspectorPanel.hpp"
#include "InspectorPanel.hpp"
#include "SceneHierarchyPanel.hpp"
#include "LaunchTool.hpp"
#include "ImGuizmo.h"

#include "Panels.hpp"

#include <imgui_internal.h>

using namespace Hydrogen;

ImGuiTextureCache TextureCache;
VkSampler ImguiSampler;

std::shared_ptr<Scene> SavedScene;

class ToolApp : public Application
{
private:
	// ============================================================
	// Editor State & UI
	// ============================================================
	bool m_IsSimulating = false;
	ImGuizmo::OPERATION m_GuizmoTool = ImGuizmo::TRANSLATE;

	DockspaceManager EditorGUI;

	// Viewport Data
	ImVec2 m_SceneViewportSize = { 1920, 1080 };
	ImVec2 m_SceneViewportBounds[2];
	bool m_SceneViewportVisible = false;
	bool m_SceneViewportHovered = false;

	ImVec2 m_GameViewportSize = { 1920, 1080 };
	bool m_GameViewportVisible = false;

	VkImageView m_EngineLogoView = VK_NULL_HANDLE;

	// ============================================================
	// Performance & Hardware
	// ============================================================
	float m_FPSTimer = 0.0f;
	float m_CurrentAvgFPS = 0.0f;
	float m_CurrentMinFPS = 0.0f;
	std::vector<float> m_FrameTimes;
	const size_t m_MaxSamples = 100;

	std::vector<RenderDeviceDescriptor> m_AvailableDevices;
	size_t m_SelectedDeviceIndex = 0;
	bool m_RenderDeviceChanged = false;
	RenderDeviceDescriptor m_NewRenderDevice = {};

	SwapChainSpec m_CurrentSwapChainSpec = {};
	bool m_SwapChainChanged = false;

	// ============================================================
	// Rendering & Camera
	// ============================================================
	RenderSettings m_RenderSettings = {};
	FreeCamera m_FreeCam;

	std::unique_ptr<Renderer> m_SceneViewportRenderer;
	std::unique_ptr<Renderer> m_GameViewportRenderer;
	std::unique_ptr<Renderer> m_ImGuiRenderer;

	VkImageView m_ViewportFinalScene = VK_NULL_HANDLE;
	VkImageView m_SceneViewportFinalScene = VK_NULL_HANDLE;

public:
	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Editor";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Editor";
		ApplicationSpec.ViewportSize = { 1920, 1080 };
		ApplicationSpec.UseDebugGUI = true;
	}

	virtual void OnStartup() override
	{
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		std::filesystem::path fontPath = std::filesystem::path(MainAssetManager.GetAssetDirectory()) / "Editor/Montserrat-Regular.ttf";
		ImFont* customFont = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 16.0f);

		if (customFont == nullptr)
			HY_APP_WARN("Failed to load custom font! Falling back to default ImGui font.");
		else
			io.FontDefault = customFont;

		auto logoAsset = MainAssetManager.GetAsset<TextureAsset>("engine_logo_taskbar.png");
		m_EngineLogoView = logoAsset->GetTexture(ActiveRenderDevice.get())->GetImageView();

		SavedScene = std::make_shared<Scene>();

		m_FreeCam.ViewportWidth = MainViewport->GetWidth();
		m_FreeCam.ViewportHeight = MainViewport->GetHeight();
		m_FreeCam.SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
		m_FreeCam.CalculateProj();
		m_FreeCam.Active = true;

		m_SceneViewportRenderer = std::make_unique<Renderer>(ActiveRenderDevice.get());
		m_GameViewportRenderer = std::make_unique<Renderer>(ActiveRenderDevice.get());

		m_ImGuiRenderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());

		ImguiSampler = m_ImGuiRenderer->GetImguiSampler();

		CurrentScene->GetScene()->CreateScripts();

		if (RenderInstance::Get())
		{
			m_AvailableDevices = RenderInstance::Get()->GetRenderDevices();
			m_SelectedDeviceIndex = GetCurrentRenderDeviceDesc().ID;
			m_CurrentSwapChainSpec = GetCurrentSwapChainSepc();
		}

		EditorGUI.Init("MainEngineDockspace");

		EditorGUI.GetEventBus().Subscribe<SceneViewportStateEvent>([this](const SceneViewportStateEvent& e) {
			m_SceneViewportSize = { e.Size.x, e.Size.y };
			m_SceneViewportBounds[0] = { e.BoundsMin.x, e.BoundsMin.y };
			m_SceneViewportBounds[1] = { e.BoundsMax.x, e.BoundsMax.y };
			m_SceneViewportVisible = e.IsVisible;
			m_SceneViewportHovered = e.IsHovered;
			});

		EditorGUI.GetEventBus().Subscribe<GameViewportStateEvent>([this](const GameViewportStateEvent& e) {
			m_GameViewportSize = { e.Size.x, e.Size.y };
			m_GameViewportVisible = e.IsVisible;
			});

		// --- Setup MenuBar Callbacks ---
		EditorGUI.SetMenuBarCallback([this]() {
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Save Scene")) CurrentScene->Save();
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Game"))
			{
				if (ImGui::MenuItem(m_IsSimulating ? "Stop" : "Play")) ToggleSimulation();
				ImGui::EndMenu();
			}
			});

		// --- Setup ToolBar Callbacks ---
		EditorGUI.SetToolBarCallback([this]() {
			float buttonHeight = 23.0f;

			ImGui::Image(TextureCache.GetTextureID(m_EngineLogoView, ImguiSampler), ImVec2(buttonHeight, buttonHeight));
			ImGui::SameLine();

			if (ImGui::Button(m_IsSimulating ? "STOP" : "PLAY", ImVec2(buttonHeight * 2.0f, buttonHeight)))
				ToggleSimulation();

			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();

			if (ImGui::Button("Launch Tracy", ImVec2(0.0f, buttonHeight)))
				LaunchTool("tracy-profiler.exe", "-a 127.0.0.1", std::filesystem::current_path().string());

			ImGui::SameLine();

			if (ImGui::Button("Launch Hydrogen Tools", ImVec2(0.0f, buttonHeight)))
				LaunchTool("HydrogenTools.exe", "", (GetCurrentExecutablePath().parent_path()).string());

			ImGui::SameLine();
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine();

			if (ImGui::RadioButton("Translate", m_GuizmoTool == ImGuizmo::TRANSLATE)) m_GuizmoTool = ImGuizmo::TRANSLATE;
			ImGui::SameLine();
			if (ImGui::RadioButton("Rotate", m_GuizmoTool == ImGuizmo::ROTATE))       m_GuizmoTool = ImGuizmo::ROTATE;
			ImGui::SameLine();
			if (ImGui::RadioButton("Scale", m_GuizmoTool == ImGuizmo::SCALE))        m_GuizmoTool = ImGuizmo::SCALE;
			});

		// --- Register Modular Panels ---
		EditorGUI.RegisterPanel<AssetBrowserPanel>();
		EditorGUI.RegisterPanel<AssetInspectorPanel>();
		EditorGUI.RegisterPanel<InspectorPanel>();
		EditorGUI.RegisterPanel<SceneHierarchyPanel>();

		EditorGUI.RegisterPanel<ConsoleLogPanel>();
		EditorGUI.RegisterPanel<PerformanceStatsPanel>(&m_CurrentAvgFPS, &m_CurrentMinFPS);
		EditorGUI.RegisterPanel<GraphicsSettingsPanel>(
			&m_AvailableDevices, &m_SelectedDeviceIndex, &m_RenderDeviceChanged,
			&m_NewRenderDevice, &m_CurrentSwapChainSpec, &m_SwapChainChanged, &m_RenderSettings
		);

		EditorGUI.RegisterPanel<SceneViewportPanel>();
		EditorGUI.RegisterPanel<GameViewportPanel>();

		EditorGUI.GetEventBus().Publish<SceneChangeEvent>({ CurrentScene->GetScene() });
	}

	virtual void OnShutdown() override
	{
		EditorGUI.Shutdown();

		SavedScene.reset();
		TextureCache.Clear();
		m_SceneViewportRenderer.reset();
		m_GameViewportRenderer.reset();
		m_ImGuiRenderer.reset();
	}

	virtual void OnUpdate(float deltaTime) override
	{
		HandleHardwareChanges();
		CalculateFPS(deltaTime);

		CurrentScene->GetScene()->RenderPhysicsDebug();

		if (m_IsSimulating)
			PhysicsUpdate(deltaTime);

		m_RenderSettings.Display.RenderToSwapChain = false;

		EditorGUI.OnUpdate(deltaTime);

		m_SceneViewportFinalScene = VK_NULL_HANDLE;
		if (m_SceneViewportVisible && m_SceneViewportSize.x != 0 && m_SceneViewportSize.y != 0)
		{
			if (Input::IsMouseButtonDown(KeyCode::MouseRight) && m_SceneViewportHovered)
			{
				Viewport::ConfineCursor(
					m_SceneViewportBounds[0].x, m_SceneViewportBounds[1].x,
					m_SceneViewportBounds[0].y, m_SceneViewportBounds[1].y
				);
				m_FreeCam.Update(deltaTime);
			}
			else
			{
				Viewport::ReleaseCursor();
			}

			m_FreeCam.CalculateView();
			UpdateCameraViewportSize(m_FreeCam, { (int)m_SceneViewportSize.x, (int)m_SceneViewportSize.y });

			m_RenderSettings.Display.Width = (uint64_t)m_SceneViewportSize.x;
			m_RenderSettings.Display.Height = (uint64_t)m_SceneViewportSize.y;

			m_SceneViewportFinalScene = DefaultRenderer::RenderSceneDeferred(
				m_SceneViewportRenderer.get(), m_RenderSettings, m_FreeCam, m_FreeCam.GetPosition(), CurrentScene->GetScene()
			).ImageView;
		}

		EditorGUI.GetEventBus().Publish(SceneViewportRenderedEvent{ m_SceneViewportFinalScene });

		m_ViewportFinalScene = VK_NULL_HANDLE;
		Entity cameraEntity;
		if (m_GameViewportVisible && m_GameViewportSize.x != 0 && m_GameViewportSize.y != 0 && UpdateCamera(CurrentScene->GetScene(), cameraEntity, m_GameViewportSize.x, m_GameViewportSize.y))
		{
			const auto& camera = cameraEntity.GetComponent<CameraComponent>();
			const auto& cameraPos = cameraEntity.GetComponent<TransformComponent>().GetPosition();

			m_RenderSettings.Display.Width = (uint64_t)m_GameViewportSize.x;
			m_RenderSettings.Display.Height = (uint64_t)m_GameViewportSize.y;

			m_ViewportFinalScene = DefaultRenderer::RenderSceneDeferred(
				m_GameViewportRenderer.get(), m_RenderSettings, camera, cameraPos, CurrentScene->GetScene()
			).ImageView;
		}

		EditorGUI.GetEventBus().Publish(GameViewportRenderedEvent{ m_ViewportFinalScene });

		DefaultRenderer::RenderImGui(m_ImGuiRenderer.get(), ActiveSwapChain.get());
	}

	virtual void OnImGuiRender() override
	{
		m_ImGuiRenderer->BeginImGuiFrame();
		ImGuizmo::BeginFrame();

		EditorGUI.OnImGuiRender();
	}

private:
	void ToggleSimulation()
	{
		if (m_IsSimulating)
		{
			CurrentScene->ClearScene();
			CurrentScene->GetScene()->DeserializeScene(SavedScene->SerializeScene(), &MainAssetManager);
			m_IsSimulating = false;
		}
		else
		{
			SavedScene = std::make_shared<Scene>();
			SavedScene->DeserializeScene(CurrentScene->GetScene()->SerializeScene(), &MainAssetManager);
			CurrentScene->GetScene()->CreateScripts();
			m_IsSimulating = true;
		}

		EditorGUI.GetEventBus().Publish<SceneChangeEvent>({ CurrentScene->GetScene() });
	}

	void HandleHardwareChanges()
	{
		if (m_RenderDeviceChanged)
		{
			ChangeRenderDevice(m_NewRenderDevice);
			m_RenderDeviceChanged = false;
		}
		if (m_SwapChainChanged)
		{
			RecreateSwapchain(m_CurrentSwapChainSpec);
			m_SwapChainChanged = false;
		}
	}

	template<typename TCamera>
	void UpdateCameraViewportSize(TCamera& camera, const glm::ivec2& size)
	{
		if (size.x > 0 && size.y > 0 && (camera.ViewportWidth != size.x || camera.ViewportHeight != size.y))
		{
			camera.ViewportWidth = size.x;
			camera.ViewportHeight = size.y;
			camera.CalculateProj();
		}
	}

	bool UpdateCamera(Scene* scene, Entity& outEntity, uint32_t viewportWidth, uint32_t viewportHeight)
	{
		Entity activeCameraEntity;
		scene->IterateComponents<CameraComponent>([&](Entity entity, CameraComponent& camera) {
			if (camera.Active) activeCameraEntity = entity;
			});

		if (activeCameraEntity.IsValid())
		{
			auto& camera = activeCameraEntity.GetComponent<CameraComponent>();
			camera.CalculateView(activeCameraEntity);
			UpdateCameraViewportSize(camera, { (int)viewportWidth, (int)viewportHeight });
			outEntity = activeCameraEntity;
			return true;
		}
		return false;
	}

	void CalculateFPS(float dt)
	{
		m_FrameTimes.push_back(dt);
		if (m_FrameTimes.size() > m_MaxSamples) m_FrameTimes.erase(m_FrameTimes.begin());

		m_FPSTimer += dt;
		if (m_FPSTimer >= 0.5f && !m_FrameTimes.empty())
		{
			float sum = std::accumulate(m_FrameTimes.begin(), m_FrameTimes.end(), 0.0f);
			float avgDelta = sum / m_FrameTimes.size();
			m_CurrentAvgFPS = avgDelta > 0.0f ? 1.0f / avgDelta : 0.0f;

			float maxDelta = *std::max_element(m_FrameTimes.begin(), m_FrameTimes.end());
			m_CurrentMinFPS = maxDelta > 0.0f ? 1.0f / maxDelta : 0.0f;
			m_FPSTimer = 0.0f;
		}
	}

	virtual void OnSwapchainRecreation() override
	{
		TextureCache.Clear();
		m_ImGuiRenderer->UpdateSwapChain(ActiveSwapChain.get());
	}

	virtual void OnRenderDeviceChangeStart() override
	{
		TextureCache.Clear();
		m_SceneViewportRenderer.reset();
		m_GameViewportRenderer.reset();
		m_ImGuiRenderer.reset();
	}

	virtual void OnRenderDeviceChangeFinish() override
	{
		m_SceneViewportRenderer = std::make_unique<Renderer>(ActiveRenderDevice.get());
		m_GameViewportRenderer = std::make_unique<Renderer>(ActiveRenderDevice.get());
		m_ImGuiRenderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());
	}
};

extern std::shared_ptr<Application> GetApplication()
{
	return std::make_shared<ToolApp>();
}
