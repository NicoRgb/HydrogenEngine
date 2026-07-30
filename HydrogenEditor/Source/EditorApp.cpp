#include <Hydrogen/HydrogenMain.hpp>

#include "Panels/AssetBrowserPanel.hpp"
#include "Panels/AssetInspectorPanel.hpp"
#include "Panels/InspectorPanel.hpp"
#include "Panels/SceneHierarchyPanel.hpp"
#include "Panels/ConsoleLogPanel.hpp"
#include "Panels/PerformanceStatsPanel.hpp"
#include "Panels/GraphicsSettingsPanel.hpp"
#include "Panels/SceneViewportPanel.hpp"
#include "Panels/GameViewportPanel.hpp"

#include "LaunchTool.hpp"

#include <imgui_internal.h>

using namespace Hydrogen;

ImGuiTextureCache TextureCache;
VkSampler ImguiSampler;

std::shared_ptr<Scene> SavedScene;

class ToolApp : public Application
{
private:
	bool m_IsSimulating = false;
	HardwareChangeEvent HardwareChange;

	DockspaceManager EditorGUI;
	std::unique_ptr<Renderer> m_ImGuiRenderer;
	VkImageView m_EngineLogoView = VK_NULL_HANDLE;

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
		m_ImGuiRenderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());

		ImguiSampler = m_ImGuiRenderer->GetImguiSampler();

		EditorGUI.Init("MainEngineDockspace");

		EditorGUI.GetEventBus().Subscribe<HardwareChangeEvent>([this](const HardwareChangeEvent& e) {
			HardwareChange = e;
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
		EditorGUI.AddToolBarCallback([this]() {
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
			});

		// --- Register Modular Panels ---
		EditorGUI.RegisterPanel<AssetBrowserPanel>();
		EditorGUI.RegisterPanel<AssetInspectorPanel>();
		EditorGUI.RegisterPanel<InspectorPanel>();
		EditorGUI.RegisterPanel<SceneHierarchyPanel>();

		EditorGUI.RegisterPanel<ConsoleLogPanel>();
		EditorGUI.RegisterPanel<PerformanceStatsPanel>();
		EditorGUI.RegisterPanel<GraphicsSettingsPanel>();

		EditorGUI.RegisterPanel<SceneViewportPanel>();
		EditorGUI.RegisterPanel<GameViewportPanel>();

		EditorGUI.GetEventBus().Publish<SceneChangeEvent>({ CurrentScene->GetScene() });
	}

	virtual void OnShutdown() override
	{
		EditorGUI.Shutdown();

		SavedScene.reset();
		TextureCache.Clear();
		m_ImGuiRenderer.reset();
	}

	virtual void OnUpdate(float deltaTime) override
	{
		HandleHardwareChanges();

		if (m_IsSimulating)
			PhysicsUpdate(deltaTime);

		EditorGUI.OnUpdate(deltaTime);
		DefaultRenderer::RenderImGui(m_ImGuiRenderer.get(), ActiveSwapChain.get());
	}

	virtual void OnImGuiRender() override
	{
		HardwareChange = {};

		m_ImGuiRenderer->BeginImGuiFrame();
		ImGuizmo::BeginFrame();

		EditorGUI.OnImGuiRender();
	}

private:
	void ToggleSimulation()
	{
		if (m_IsSimulating)
		{
			CurrentScene->GetScene()->ResetPhysics();
			CurrentScene->ClearScene();
			CurrentScene->GetScene()->DeserializeScene(SavedScene->SerializeScene());
			m_IsSimulating = false;
		}
		else
		{
			json j = CurrentScene->GetScene()->SerializeScene();

			if (SavedScene) SavedScene->ResetPhysics();
			SavedScene = std::make_shared<Scene>();
			SavedScene->DeserializeScene(j);

			CurrentScene->GetScene()->ResetPhysics();
			CurrentScene->ClearScene();
			CurrentScene->GetScene()->DeserializeScene(j);

			m_IsSimulating = true;
			ResetPhysicsAccumulator();
		}

		EditorGUI.GetEventBus().Publish<SceneChangeEvent>({ CurrentScene->GetScene() });
	}

	void HandleHardwareChanges()
	{
		if (HardwareChange.RenderDeviceChanged)
		{
			ChangeRenderDevice(HardwareChange.RenderDeviceDesc);
			HardwareChange.RenderDeviceChanged = false;
		}
		if (HardwareChange.SwapChainChanged)
		{
			RecreateSwapchain(HardwareChange.SwapChainSpec);
			HardwareChange.SwapChainChanged = false;
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
		m_ImGuiRenderer.reset();
	}

	virtual void OnRenderDeviceChangeFinish() override
	{
		m_ImGuiRenderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());
	}
};

extern std::shared_ptr<Application> GetApplication()
{
	return std::make_shared<ToolApp>();
}
