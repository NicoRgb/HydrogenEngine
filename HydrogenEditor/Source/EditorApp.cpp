#include <Hydrogen/HydrogenMain.hpp>

#include "AssetBrowserPanel.hpp"
#include "AssetEditorPanel.hpp"
#include "InspectorPanel.hpp"
#include "SceneHierarchyPanel.hpp"
#include "ImGuizmo.h"

#include <imgui_internal.h>

using namespace Hydrogen;

static AssetEditorPanel  _EditorPanel;
static AssetBrowserPanel _BrowserPanel("assets", _EditorPanel);
static SceneHierarchyPanel _SceneHierarchy;
static InspectorPanel    _Inspector;

ImGuiTextureCache TextureCache;
std::shared_ptr<Scene> SavedScene;

class EditorApp : public Application
{
private:
	// ============================================================
	// Editor State & UI
	// ============================================================
	bool m_IsSimulating = false;
	ImGuizmo::OPERATION m_GuizmoTool = ImGuizmo::TRANSLATE;

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
	bool m_WireframeMode = false;
	FreeCamera m_FreeCam;

	std::unique_ptr<Renderer> m_ViewportRenderer;
	std::unique_ptr<Renderer> m_ImGuiRenderer;

	VkImageView m_ViewportFinalScene = VK_NULL_HANDLE;
	VkImageView m_SceneViewportFinalScene = VK_NULL_HANDLE;

	struct UniformBuffer
	{
		glm::mat4 View;
		glm::mat4 Proj;
		glm::vec3 ViewPos;
		float Padding;
	};

	struct PushConstants
	{
		glm::mat4 Model;
		glm::vec4 Color;
		int TexIndex;
		int Padding[3];
	};

public:
	// ============================================================
	// Application Lifecycle
	// ============================================================

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
		{
			HY_APP_WARN("Failed to load custom font! Falling back to default ImGui font.");
		}
		else
		{
			io.FontDefault = customFont;
		}

		ApplyHydrogenTheme();

		auto folderTextureAsset = MainAssetManager.GetAsset<TextureAsset>("folder_icon.png");
		auto fileTextureAsset = MainAssetManager.GetAsset<TextureAsset>("file_icon.png");

		auto logoAsset = MainAssetManager.GetAsset<TextureAsset>("engine_logo_taskbar.png");
		m_EngineLogoView = logoAsset->GetTexture(ActiveRenderDevice.get())->GetImageView();

		_BrowserPanel.LoadTextures(
			folderTextureAsset->GetTexture(ActiveRenderDevice.get()),
			fileTextureAsset->GetTexture(ActiveRenderDevice.get())
		);

		_SceneHierarchy.SetContext(CurrentScene->GetScene());
		SavedScene = std::make_shared<Scene>();

		m_FreeCam.ViewportWidth = MainViewport->GetWidth();
		m_FreeCam.ViewportHeight = MainViewport->GetHeight();
		m_FreeCam.SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
		m_FreeCam.CalculateProj();
		m_FreeCam.Active = true;

		m_ViewportRenderer = std::make_unique<Renderer>(ActiveRenderDevice.get());
		m_ImGuiRenderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());

		CurrentScene->GetScene()->CreateScripts();

		if (RenderInstance::Get())
		{
			m_AvailableDevices = RenderInstance::Get()->GetRenderDevices();
			m_SelectedDeviceIndex = GetCurrentRenderDeviceDesc().ID;
			m_CurrentSwapChainSpec = GetCurrentSwapChainSepc();
		}

		_BrowserPanel.Setup();
	}

	virtual void OnShutdown() override
	{
		SavedScene.reset();
		_BrowserPanel.Shutdown();
		TextureCache.Clear();
		m_ViewportRenderer.reset();
		m_ImGuiRenderer.reset();
	}

	virtual void OnUpdate(float deltaTime) override
	{
		HandleHardwareChanges();
		CalculateFPS(deltaTime);

		RenderSettings renderSettings;
		renderSettings.Display.Width = m_GameViewportSize.x;
		renderSettings.Display.Height = m_GameViewportSize.y;

		renderSettings.Debug.WireframeMode = m_WireframeMode;

		CurrentScene->GetScene()->RenderPhysicsDebug();

		if (m_IsSimulating)
			PhysicsUpdate(deltaTime);

		// Update Game Viewport
		Entity cameraEntity;
		if (m_GameViewportVisible && UpdateCamera(CurrentScene->GetScene(), cameraEntity, m_GameViewportSize.x, m_GameViewportSize.y))
		{
			const auto& camera = cameraEntity.GetComponent<CameraComponent>();
			const auto& cameraPos = cameraEntity.GetComponent<TransformComponent>().GetPosition();

			m_ViewportFinalScene = DefaultRenderer::RenderScene(m_ViewportRenderer.get(), renderSettings, camera, cameraPos, CurrentScene->GetScene()).ImageView;
		}

		// Update Scene Viewport
		if (m_SceneViewportVisible)
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

			m_SceneViewportFinalScene = DefaultRenderer::RenderScene(m_ViewportRenderer.get(), renderSettings, m_FreeCam, m_FreeCam.GetPosition(), CurrentScene->GetScene()).ImageView;
		}

		DefaultRenderer::RenderImGui(m_ImGuiRenderer.get(), ActiveSwapChain.get());
	}

	virtual void OnImGuiRender() override
	{
		m_ImGuiRenderer->BeginImGuiFrame();

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		float toolbarHeight = 38.0f;

		DrawToolbar(viewport->WorkPos, ImVec2(viewport->WorkSize.x, toolbarHeight));

		ImVec2 dockspacePos = { viewport->WorkPos.x, viewport->WorkPos.y + toolbarHeight };
		ImVec2 dockspaceSize = { viewport->WorkSize.x, viewport->WorkSize.y - toolbarHeight };

		SetupDockspace(dockspacePos, dockspaceSize, viewport->ID);

		DrawViewports();
		DrawLogMessages();

		_BrowserPanel.OnImGuiRender();
		_EditorPanel.OnImGuiRender();
		_SceneHierarchy.OnImGuiRender();

		_Inspector.SetContext(CurrentScene->GetScene(), _SceneHierarchy.GetSelectedEntity());
		_Inspector.OnImGuiRender();

		DrawPerformanceStats();
		DrawGraphicsSettings();
	}

private:
	// ============================================================
	// UI Rendering Methods
	// ============================================================

	void SetupDockspace(ImVec2 pos, ImVec2 size, ImGuiID viewportId)
	{
		static bool dockingEnabled = true;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize(size);
		ImGui::SetNextWindowViewport(viewportId);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGui::Begin("MainDockSpace", &dockingEnabled, window_flags);
		ImGui::PopStyleVar(2);

		ImGuiID dockspace_id = ImGui::GetID("DockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

		DrawMenuBar();
		ImGui::End();
	}

	void DrawMenuBar()
	{
		if (ImGui::BeginMenuBar())
		{
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
			ImGui::EndMenuBar();
		}
	}

	void DrawToolbar(ImVec2 pos, ImVec2 size)
	{
		ImGui::SetNextWindowPos(pos);
		ImGui::SetNextWindowSize(size);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoDocking;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGui::Begin("##EngineToolbar", nullptr, flags);

		ImGui::Image(TextureCache.GetTextureID(m_EngineLogoView, m_ViewportRenderer->GetImguiSampler()), ImVec2(size.y * 0.90, size.y * 0.90));

		float buttonHeight = ImGui::GetWindowHeight() - 12.0f;
		ImGui::SameLine();

		// --- Simulation Controls ---
		if (ImGui::Button(m_IsSimulating ? "STOP" : "PLAY", ImVec2(buttonHeight * 2.0f, buttonHeight)))
			ToggleSimulation();

		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		// --- Gizmo Transform Selectors ---
		if (ImGui::RadioButton("Translate", m_GuizmoTool == ImGuizmo::TRANSLATE)) m_GuizmoTool = ImGuizmo::TRANSLATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Rotate", m_GuizmoTool == ImGuizmo::ROTATE))       m_GuizmoTool = ImGuizmo::ROTATE;
		ImGui::SameLine();
		if (ImGui::RadioButton("Scale", m_GuizmoTool == ImGuizmo::SCALE))         m_GuizmoTool = ImGuizmo::SCALE;

		ImGui::End();
		ImGui::PopStyleVar(2);
	}

	void DrawViewports()
	{
		// --- SCENE VIEWPORT ---
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		m_SceneViewportVisible = ImGui::Begin("Scene");

		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_SceneViewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_SceneViewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

		m_SceneViewportHovered = ImGui::IsWindowHovered();

		if (m_SceneViewportVisible)
		{
			ImVec2 contentRegion = ImGui::GetContentRegionAvail();

			if (m_SceneViewportSize.x != contentRegion.x || m_SceneViewportSize.y != contentRegion.y)
				m_SceneViewportSize = { contentRegion.x, contentRegion.y };

			if (m_SceneViewportFinalScene != VK_NULL_HANDLE)
			{
				ImGui::Image(TextureCache.GetTextureID(m_SceneViewportFinalScene, m_ViewportRenderer->GetImguiSampler()), m_SceneViewportSize);
				DrawGizmo();
			}
		}
		ImGui::End();
		ImGui::PopStyleVar();

		// --- GAME VIEWPORT ---
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		m_GameViewportVisible = ImGui::Begin("Game");

		if (m_GameViewportVisible)
		{
			ImVec2 contentRegion = ImGui::GetContentRegionAvail();
			if (m_GameViewportSize.x != contentRegion.x || m_GameViewportSize.y != contentRegion.y)
				m_GameViewportSize = { contentRegion.x, contentRegion.y };

			if (m_ViewportFinalScene != VK_NULL_HANDLE)
			{
				ImGui::Image(TextureCache.GetTextureID(m_ViewportFinalScene, m_ViewportRenderer->GetImguiSampler()), m_GameViewportSize);
			}
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	void DrawGizmo()
	{
		auto selectedEntity = _SceneHierarchy.GetSelectedEntity();
		if (!selectedEntity.IsValid()) return;

		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(m_SceneViewportBounds[0].x, m_SceneViewportBounds[0].y,
			m_SceneViewportBounds[1].x - m_SceneViewportBounds[0].x,
			m_SceneViewportBounds[1].y - m_SceneViewportBounds[0].y);

		auto& tc = selectedEntity.GetComponent<TransformComponent>();
		glm::mat4 transform = tc.Transform;
		glm::mat4 view = m_FreeCam.View;
		glm::mat4 proj = m_FreeCam.Proj;
		proj[1][1] *= -1;

		ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
			m_GuizmoTool, ImGuizmo::WORLD, glm::value_ptr(transform));

		if (ImGuizmo::IsUsing())
			tc.Transform = transform;
	}

	// ============================================================
	// Simulation & Helpers
	// ============================================================

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
		_SceneHierarchy.SetContext(CurrentScene->GetScene());
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

	// ============================================================
	// Secondary UI Panels
	// ============================================================

	void DrawLogMessages()
	{
		ImGui::Begin("Console Output");
		ImGui::BeginChild("LogScrollRegion");

		auto drawSink = [&](auto logger) {
			for (auto& m : logger->GetLogSink()->GetMessages())
			{
				ImVec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
				if (m.level == spdlog::level::debug) color = { 0.6f, 0.6f, 1.0f, 1.0f };
				else if (m.level == spdlog::level::warn) color = { 1.0f, 1.0f, 0.1f, 1.0f };
				else if (m.level == spdlog::level::err) color = { 1.0f, 0.3f, 0.3f, 1.0f };
				else if (m.level == spdlog::level::critical) color = { 1.0f, 0.0f, 0.0f, 1.0f };

				ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextUnformatted(m.message.c_str());
				ImGui::PopStyleColor();
			}
			};

		drawSink(EngineLogger::GetLogger());
		drawSink(AppLogger::GetLogger());

		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
		ImGui::End();
	}

	void DrawPerformanceStats()
	{
		ImGui::Begin("Performance Statistics");
		ImGui::Text("Average FPS: %.1f", m_CurrentAvgFPS);
		ImGui::Text("Minimum FPS: %.1f", m_CurrentMinFPS);

		if (m_CurrentAvgFPS > 0.0f)
		{
			ImGui::Separator();
			ImGui::Text("Frame Time: %.2f ms", (1000.0f / m_CurrentAvgFPS));
		}

		VmaTotalStatistics stats{};
		vmaCalculateStatistics(ActiveRenderDevice->GetAllocator(), &stats);
		ImGui::Separator();
		ImGui::Text("Memory Usage:");
		ImGui::Text("  Allocations: %zu", stats.total.statistics.allocationCount);
		ImGui::Text("  Allocated: %.2f MB", static_cast<double>(stats.total.statistics.allocationBytes) / (1024.0 * 1024.0));
		ImGui::End();
	}

	void DrawGraphicsSettings()
	{
		ImGui::Begin("Graphics Settings");

		ImGui::TextDisabled("HARDWARE CONFIGURATION");
		if (m_AvailableDevices.empty())
		{
			ImGui::Text("No valid rendering devices found!");
		}
		else
		{
			const auto& currentDevice = m_AvailableDevices[m_SelectedDeviceIndex];
			std::string previewName = currentDevice.Name;

			ImGui::Text("Target Graphics Hardware:");

			if (ImGui::BeginCombo("##DeviceSelector", previewName.c_str()))
			{
				for (size_t i = 0; i < m_AvailableDevices.size(); ++i)
				{
					const auto& device = m_AvailableDevices[i];

					double vramGB = static_cast<double>(device.VramBytes) / (1024.0 * 1024.0 * 1024.0);

					std::string typeStr = (device.Type == RenderDeviceType::DiscreteGPU) ? "Discrete" : "Integrated";
					std::string label = device.Name + " (" + typeStr + " - " + std::to_string(vramGB).substr(0, 4) + " GB)";

					bool isSelected = (m_SelectedDeviceIndex == i);
					if (ImGui::Selectable(label.c_str(), isSelected))
					{
						m_SelectedDeviceIndex = i;
						HY_APP_INFO("Selected Device {}", device.Name);
						m_RenderDeviceChanged = true;
						m_NewRenderDevice = device;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::Text("Device Specifications:");
			ImGui::Text("  Internal ID: %zu", currentDevice.ID);
			ImGui::Text("  Total VRAM:  %.2f GB", static_cast<double>(currentDevice.VramBytes) / (1024.0 * 1024.0 * 1024.0));
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("SWAPCHAIN & DISPLAY PREFERENCES");

		const char* presentModeLabels[] = { "Immediate (Uncapped / Tearing)", "VSync (Capped)", "Mailbox (Triple Buffering)" };
		int currentPresentInt = static_cast<int>(m_CurrentSwapChainSpec.VsyncPreference);

		ImGui::Text("Vertical Sync Mode:");
		if (ImGui::BeginCombo("##VsyncSelector", presentModeLabels[currentPresentInt]))
		{
			for (int i = 0; i < 3; ++i)
			{
				bool isSelected = (currentPresentInt == i);
				if (ImGui::Selectable(presentModeLabels[i], isSelected))
				{
					m_CurrentSwapChainSpec.VsyncPreference = static_cast<PresentMode>(i);
					HY_APP_INFO("Present Mode changed to: {}", presentModeLabels[i]);
					m_SwapChainChanged = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();

		const char* colorFormatLabels[] = { "RGBA8 SRGB", "BGRA8 SRGB", "HDR10 (High Dynamic Range)" };
		int currentColorInt = static_cast<int>(m_CurrentSwapChainSpec.ColorPreference);

		ImGui::Text("Color Format Profile:");
		if (ImGui::BeginCombo("##ColorSelector", colorFormatLabels[currentColorInt]))
		{
			for (int i = 0; i < 3; ++i)
			{
				bool isSelected = (currentColorInt == i);
				if (ImGui::Selectable(colorFormatLabels[i], isSelected))
				{
					m_CurrentSwapChainSpec.ColorPreference = static_cast<ColorFormat>(i);
					HY_APP_INFO("Color Format profile changed to: {}", colorFormatLabels[i]);
					m_SwapChainChanged = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("RENDERING SETTINGS");
		ImGui::Checkbox("Wireframe Mode", &m_WireframeMode);
		ImGui::Spacing();

		ImGui::End();
	}

	// ============================================================
	// Custom ImGui Theme
	// ============================================================

	void ApplyHydrogenTheme()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		// --- GEOMETRY & SPACING ---
		style.WindowRounding = 0.0f;
		style.ChildRounding = 0.0f;
		style.FrameRounding = 0.0f;
		style.PopupRounding = 0.0f;
		style.ScrollbarRounding = 0.0f;
		style.GrabRounding = 0.0f;
		style.TabRounding = 0.0f;

		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 1.0f;
		style.PopupBorderSize = 1.0f;
		style.TabBorderSize = 1.0f;

		// --- COLOR PALETTE ---
		// Hex: #3DDCFF -> (61, 220, 255) -> (0.24f, 0.86f, 1.00f) [Electric Cyan]
		ImVec4 electricCyan = ImVec4(0.24f, 0.86f, 1.00f, 1.00f);
		ImVec4 electricCyanMuted = ImVec4(0.24f, 0.86f, 1.00f, 0.60f);

		// Hex: #0A3D91 -> (10, 61, 145) -> (0.04f, 0.24f, 0.57f) [Deep Blue]
		ImVec4 deepBlue = ImVec4(0.04f, 0.24f, 0.57f, 1.00f);
		ImVec4 deepBlueHover = ImVec4(0.06f, 0.30f, 0.68f, 1.00f);

		// Ultra-dark blue-tinted bases for the engine background
		ImVec4 bgBase = ImVec4(0.05f, 0.06f, 0.08f, 1.00f);
		ImVec4 bgSurface = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
		ImVec4 border = ImVec4(0.12f, 0.14f, 0.18f, 1.00f);

		colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

		// Backgrounds
		colors[ImGuiCol_WindowBg] = bgBase;
		colors[ImGuiCol_ChildBg] = bgBase;
		colors[ImGuiCol_PopupBg] = bgSurface;
		colors[ImGuiCol_Border] = border;
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f); // No shadows

		// Frames (Text boxes, Checkboxes, etc.)
		colors[ImGuiCol_FrameBg] = bgSurface;
		colors[ImGuiCol_FrameBgHovered] = deepBlue;
		colors[ImGuiCol_FrameBgActive] = electricCyan;

		// Titles
		colors[ImGuiCol_TitleBg] = bgSurface;
		colors[ImGuiCol_TitleBgActive] = bgSurface;
		colors[ImGuiCol_TitleBgCollapsed] = bgBase;

		// Buttons
		colors[ImGuiCol_Button] = bgSurface;
		colors[ImGuiCol_ButtonHovered] = deepBlueHover;
		colors[ImGuiCol_ButtonActive] = electricCyan;

		// Sliders & Grabs
		colors[ImGuiCol_SliderGrab] = deepBlue;
		colors[ImGuiCol_SliderGrabActive] = electricCyan;
		colors[ImGuiCol_CheckMark] = electricCyan;

		// Tabs
		colors[ImGuiCol_Tab] = bgBase;
		colors[ImGuiCol_TabHovered] = deepBlueHover;
		colors[ImGuiCol_TabActive] = deepBlue;
		colors[ImGuiCol_TabUnfocused] = bgBase;
		colors[ImGuiCol_TabUnfocusedActive] = bgSurface;

		// Headers (Collapsing headers, tree nodes)
		colors[ImGuiCol_Header] = bgSurface;
		colors[ImGuiCol_HeaderHovered] = deepBlueHover;
		colors[ImGuiCol_HeaderActive] = deepBlue;

		// Docking & Resizing
		colors[ImGuiCol_Separator] = border;
		colors[ImGuiCol_SeparatorHovered] = deepBlue;
		colors[ImGuiCol_SeparatorActive] = electricCyan;
		colors[ImGuiCol_ResizeGrip] = deepBlue;
		colors[ImGuiCol_ResizeGripHovered] = deepBlueHover;
		colors[ImGuiCol_ResizeGripActive] = electricCyan;
		colors[ImGuiCol_DockingPreview] = electricCyanMuted;
	}

	// ============================================================
	// Vulkan Swapchain Hooks
	// ============================================================

	virtual void OnSwapchainRecreation() override
	{
		TextureCache.Clear();
		m_ImGuiRenderer->UpdateSwapChain(ActiveSwapChain.get());
	}

	virtual void OnRenderDeviceChangeStart() override
	{
		TextureCache.Clear();
		m_ViewportRenderer.reset();
		m_ImGuiRenderer.reset();
	}

	virtual void OnRenderDeviceChangeFinish() override
	{
		m_ViewportRenderer = std::make_unique<Renderer>(ActiveRenderDevice.get());
		m_ImGuiRenderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());
	}
};

extern std::shared_ptr<Application> GetApplication()
{
	return std::make_shared<EditorApp>();
}
