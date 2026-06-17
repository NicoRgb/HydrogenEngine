#include <Hydrogen/HydrogenMain.hpp>

#include "AssetBrowserPanel.hpp"
#include "AssetEditorPanel.hpp"
#include "InspectorPanel.hpp"
#include "SceneHierarchyPanel.hpp"
#include "ImGuizmo.h"

using namespace Hydrogen;

static AssetEditorPanel      _EditorPanel;
static AssetBrowserPanel     _BrowserPanel("assets", _EditorPanel);
static SceneHierarchyPanel   _SceneHierarchy;
static InspectorPanel        _Inspector;

static ImVec2 SceneViewportSize = { 500, 500 };
static ImVec2 SceneViewportPos = { 0, 0 };
static bool SceneViewportVisible = false;
static ImVec2 ViewportSize = { 500, 500 };
static ImVec2 ViewportPos = { 0, 0 };
static bool ViewportVisible = false;
static ImGuizmo::OPERATION GuizmoTool = ImGuizmo::TRANSLATE;

std::shared_ptr<Scene> SavedScene;

class EditorApp : public Application
{
private:

	float m_FPS = 0.0f;
	bool  m_IsSimulating = false;

	std::shared_ptr<DeferredRenderer> SceneViewportRenderer;
	std::shared_ptr<DeferredRenderer> ViewportRenderer;
	std::shared_ptr<DebugGUIRenderer> ImGuiRenderer;

	//PostProcessing SceneViewportPostProcessing;
	//PostProcessing ViewportPostProcessing;

	std::shared_ptr<DebugGUI> DebugGUI;
	FreeCamera FreeCam;
	std::shared_ptr<CubeMap> Skybox;

	RenderSettingsManager settingsManager;

private:

	// ============================================================
	// Simulation Control
	// ============================================================

	void StartSimulation()
	{
		SavedScene = std::make_shared<Scene>();
		SavedScene->DeserializeScene(
			CurrentScene->GetScene()->SerializeScene(),
			&MainAssetManager
		);

		CurrentScene->GetScene()->CreateScripts();

		m_IsSimulating = true;
	}

	void StopSimulation()
	{
		CurrentScene->ClearScene();
		CurrentScene->GetScene()->DeserializeScene(
			SavedScene->SerializeScene(),
			&MainAssetManager
		);

		m_IsSimulating = false;
	}

	void ToggleSimulation()
	{
		if (m_IsSimulating)
			StopSimulation();
		else
			StartSimulation();

		_SceneHierarchy.SetContext(CurrentScene->GetScene());
	}

	template<typename TCamera>
	void UpdateCameraViewportSize(TCamera& camera, const glm::ivec2& size)
	{
		if (camera.ViewportWidth != size.x ||
			camera.ViewportHeight != size.y)
		{
			camera.ViewportWidth = size.x;
			camera.ViewportHeight = size.y;
			camera.CalculateProj();
		}
	}

	bool UpdateCamera(const std::shared_ptr<Scene>& scene, Entity& outEntity, uint32_t viewportWidth, uint32_t viewportHeight)
	{
		Entity activeCameraEntity;

		scene->IterateComponents<CameraComponent>(
			[&](Entity entity, CameraComponent& camera)
			{
				if (camera.Active)
					activeCameraEntity = entity;
			});

		glm::ivec2 size = {
				(int)viewportWidth,
				(int)viewportHeight
		};

		if (activeCameraEntity.IsValid())
		{
			auto& camera = activeCameraEntity.GetComponent<CameraComponent>();
			camera.CalculateView(activeCameraEntity);
			UpdateCameraViewportSize(camera, size);
			outEntity = activeCameraEntity;
			return true;
		}

		return false;
	}

	// ============================================================
	// Log Rendering
	// ============================================================

	ImVec4 GetLogColor(spdlog::level::level_enum level)
	{
		switch (level)
		{
		case spdlog::level::debug:    return { 0.6f, 0.6f, 1, 1 };
		case spdlog::level::warn:     return { 1, 1, 0.1f, 1 };
		case spdlog::level::err:      return { 1, 0.3f, 0.3f, 1 };
		case spdlog::level::critical: return { 1, 0, 0, 1 };
		default:                     return { 1, 1, 1, 1 };
		}
	}

	void DrawSettingsWindow()
	{
		RenderSettings settings = settingsManager.Get();
		bool changed = false;

		if (ImGui::Begin("Renderer Settings"))
		{
			// --------------------------------------------------------------------
			// DISPLAY SETTINGS
			// --------------------------------------------------------------------
			if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
			{
				changed |= ImGui::Checkbox("Enable VSync", &settings.Display.EnableVSync);
				changed |= ImGui::Checkbox("Enable HDR", &settings.Display.EnableHDR);
				changed |= ImGui::Checkbox("Wireframe Mode", &settings.Display.EnableWireframe);
			}

			// --------------------------------------------------------------------
			// SHADOW SETTINGS
			// --------------------------------------------------------------------
			if (ImGui::CollapsingHeader("Shadows"))
			{
				changed |= ImGui::Checkbox("Enable Shadows", &settings.Shadows.EnableShadows);

				if (settings.Shadows.EnableShadows)
				{
					// Shadow Resolution Dropdown
					const char* shadowResLabels[] = { "Low (512x512)", "Medium (1024x1024)", "High (2048x2048)", "Ultra (4096x4096)" };
					int currentResIdx = 0;
					if (settings.Shadows.Resolution == ShadowResolution::Low) currentResIdx = 0;
					else if (settings.Shadows.Resolution == ShadowResolution::Medium) currentResIdx = 1;
					else if (settings.Shadows.Resolution == ShadowResolution::High) currentResIdx = 2;
					else if (settings.Shadows.Resolution == ShadowResolution::Ultra) currentResIdx = 3;

					if (ImGui::Combo("Shadow Resolution", &currentResIdx, shadowResLabels, IM_ARRAYSIZE(shadowResLabels)))
					{
						if (currentResIdx == 0) settings.Shadows.Resolution = ShadowResolution::Low;
						else if (currentResIdx == 1) settings.Shadows.Resolution = ShadowResolution::Medium;
						else if (currentResIdx == 2) settings.Shadows.Resolution = ShadowResolution::High;
						else if (currentResIdx == 3) settings.Shadows.Resolution = ShadowResolution::Ultra;
						changed = true;
					}

					changed |= ImGui::Checkbox("Soft Shadows (PCF)", &settings.Shadows.EnableSoftShadows);
				}
			}

			// --------------------------------------------------------------------
			// ANTI-ALIASING & FILTERING SETTINGS
			// --------------------------------------------------------------------
			if (ImGui::CollapsingHeader("Anti-Aliasing & Filtering"))
			{
				// Post-Process AA Dropdown
				const char* aaModes[] = { "None", "FXAA", "TAA", "SMAA" };
				int currentMode = static_cast<int>(settings.AA.Mode);
				if (ImGui::Combo("Anti-Aliasing Method", &currentMode, aaModes, IM_ARRAYSIZE(aaModes)))
				{
					settings.AA.Mode = static_cast<AntiAliasingMode>(currentMode);
					changed = true;
				}

				// Hardware MSAA Dropdown
				const char* msaaSamples[] = { "None", "2x MSAA", "4x MSAA", "8x MSAA" };
				int currentMSAA = 0;
				if (settings.AA.MSAASamples == MSAASampleCount::None) currentMSAA = 0;
				else if (settings.AA.MSAASamples == MSAASampleCount::X2) currentMSAA = 1;
				else if (settings.AA.MSAASamples == MSAASampleCount::X4) currentMSAA = 2;
				else if (settings.AA.MSAASamples == MSAASampleCount::X8) currentMSAA = 3;

				if (ImGui::Combo("MSAA Count", &currentMSAA, msaaSamples, IM_ARRAYSIZE(msaaSamples)))
				{
					if (currentMSAA == 0) settings.AA.MSAASamples = MSAASampleCount::None;
					else if (currentMSAA == 1) settings.AA.MSAASamples = MSAASampleCount::X2;
					else if (currentMSAA == 2) settings.AA.MSAASamples = MSAASampleCount::X4;
					else if (currentMSAA == 3) settings.AA.MSAASamples = MSAASampleCount::X8;
					changed = true;
				}

				changed |= ImGui::Checkbox("Anisotropic Filtering", &settings.AA.EnableAnisotropicFiltering);
			}

			// --------------------------------------------------------------------
			// LIGHTING & SCREEN SPACE EFFECTS
			// --------------------------------------------------------------------
			if (ImGui::CollapsingHeader("Lighting & GI"))
			{
				changed |= ImGui::Checkbox("Screen-Space Ambient Occlusion (SSAO)", &settings.Lighting.EnableSSAO);
				changed |= ImGui::Checkbox("Screen-Space Global Illumination (SSGI)", &settings.Lighting.EnableSSGI);
				changed |= ImGui::Checkbox("Standard Screen-Space Reflections (SSR)", &settings.Lighting.EnableSSR);
				changed |= ImGui::Checkbox("Stochastic Reflections (SSSR)", &settings.Lighting.EnableSSSR);
			}

			// --------------------------------------------------------------------
			// POST-PROCESSING SETTINGS
			// --------------------------------------------------------------------
			if (ImGui::CollapsingHeader("Post-Processing"))
			{
				changed |= ImGui::Checkbox("Enable Bloom", &settings.PostProcess.EnableBloom);
				changed |= ImGui::Checkbox("Enable Depth of Field", &settings.PostProcess.EnableDepthOfField);
				changed |= ImGui::Checkbox("Enable Motion Blur", &settings.PostProcess.EnableMotionBlur);

				ImGui::Separator();
				changed |= ImGui::Checkbox("Enable Tonemapping", &settings.PostProcess.EnableToneMapping);

				if (settings.PostProcess.EnableToneMapping)
				{
					const char* tonemappers[] = { "None", "Reinhard", "ACES", "Filmic" };
					int currentTonemapper = static_cast<int>(settings.PostProcess.Tonemapper);
					if (ImGui::Combo("Tonemap Operator", &currentTonemapper, tonemappers, IM_ARRAYSIZE(tonemappers)))
					{
						settings.PostProcess.Tonemapper = static_cast<TonemappingMethod>(currentTonemapper);
						changed = true;
					}
				}
			}

			// --------------------------------------------------------------------
			// CULLING OPTIMIZATIONS
			// --------------------------------------------------------------------
			if (ImGui::CollapsingHeader("Culling & Optimizations"))
			{
				changed |= ImGui::Checkbox("Frustum Culling", &settings.Culling.EnableFrustumCulling);
				changed |= ImGui::Checkbox("Hardware Occlusion Culling", &settings.Culling.EnableOcclusionCulling);
			}

			// --------------------------------------------------------------------
			// DEBUGGING TOOLS & OVERLAYS
			// --------------------------------------------------------------------
			if (ImGui::CollapsingHeader("Debug & Tooling"))
			{
				changed |= ImGui::Checkbox("Show Debug Overlays (GBuffer/Stats)", &settings.Debug.EnableDebugOverlays);
				changed |= ImGui::Checkbox("Enable Entity ID Picking (Editor)", &settings.Debug.EnableEntityIDPicking);
				changed |= ImGui::Checkbox("Render Editor Gizmos", &settings.Debug.EnableGizmoRendering);
			}

			// --------------------------------------------------------------------
			// SUBMIT UPDATE TO ENGINE
			// --------------------------------------------------------------------
			// Only trigger dirty flags and evaluation changes if a checkbox/combo flipped
			if (changed)
			{
				settingsManager.SetSettings(settings);
			}
		}
		ImGui::End();
	}

	void DrawLogMessages()
	{
		ImGui::Begin("Log");
		ImGui::BeginChild("LogScrollRegion");

		auto drawSink = [&](auto logger)
			{
				for (auto& m : logger->GetLogSink()->GetMessages())
				{
					ImVec4 color = GetLogColor(m.level);
					ImGui::PushStyleColor(ImGuiCol_Text, color);
					ImGui::TextUnformatted(m.message.c_str());
					ImGui::PopStyleColor();
				}
			};

		drawSink(EngineLogger::GetLogger());
		drawSink(AppLogger::GetLogger());

		ImGui::EndChild();
		ImGui::End();
	}

	// ============================================================
	// Viewport + Gizmo
	// ============================================================

	void DrawGizmo()
	{
		auto selectedEntity = _SceneHierarchy.GetSelectedEntity();
		if (!selectedEntity.IsValid())
			return;

		ImGuizmo::SetDrawlist();

		auto& tc = selectedEntity.GetComponent<TransformComponent>();
		glm::mat4 transform = tc.Transform;

		glm::mat4 view = FreeCam.View;
		glm::mat4 proj = FreeCam.Proj;
		proj[1][1] *= -1;

		ImGuizmo::Manipulate(
			glm::value_ptr(view),
			glm::value_ptr(proj),
			GuizmoTool,
			ImGuizmo::WORLD,
			glm::value_ptr(transform)
		);

		if (ImGuizmo::IsUsing())
			tc.Transform = transform;
	}

	void DrawViewports()
	{
		SceneViewportVisible = ImGui::Begin("Scene Viewport");

		if (SceneViewportVisible)
		{
			ImVec2 contentRegion = ImGui::GetContentRegionAvail();
			SceneViewportPos = ImGui::GetWindowPos();

			if (contentRegion.x != SceneViewportSize.x ||
				contentRegion.y != SceneViewportSize.y)
			{
				SceneViewportSize = contentRegion;

				SceneViewportRenderer->Resize(
					(size_t)contentRegion.x,
					(size_t)contentRegion.y);

				ImGuizmo::SetRect(
					SceneViewportPos.x,
					SceneViewportPos.y,
					contentRegion.x,
					contentRegion.y);
			}

			const auto& image = SceneViewportRenderer->GetFinalSceneTexture();
			if (image)
			{
				ImGui::Image(image->GetImGuiImage(), contentRegion);
				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
				{
					float held = ImGui::GetIO().MouseDownDurationPrev[ImGuiMouseButton_Left];

					if (held < 0.2f)
					{
						ImVec2 mousePos = ImGui::GetMousePos();

						float texX = (mousePos.x - SceneViewportPos.x) / SceneViewportSize.x;
						float texY = (mousePos.y - SceneViewportPos.y) / SceneViewportSize.y;

						uint32_t pixelX = static_cast<uint32_t>(texX * image->GetWidth());
						uint32_t pixelY = static_cast<uint32_t>(texY * image->GetHeight());

						if (pixelX < image->GetWidth() && pixelY < image->GetHeight())
						{
							uint32_t selectedEntityID = SceneViewportRenderer->ReadEntityIDFromGPU(pixelX, pixelY);
							Entity e = CurrentScene->GetScene()->GetEntityByEntityID(selectedEntityID);
							if (e.IsValid())
							{
								_SceneHierarchy.SetSelectedEntity(e.GetUUID());
								_Inspector.SetSelectedEntity(e);
							}
							else
							{
								_SceneHierarchy.SetSelectedEntity(0);
								_Inspector.SetSelectedEntity(Entity());
							}
						}
					}
				}
			}

			DrawGizmo();
		}

		ImGui::End();

		ViewportVisible = ImGui::Begin("Game Viewport");

		if (ViewportVisible)
		{
			ImVec2 contentRegion = ImGui::GetContentRegionAvail();
			ViewportPos = ImGui::GetWindowPos();

			if (contentRegion.x != ViewportSize.x ||
				contentRegion.y != ViewportSize.y)
			{
				ViewportSize = contentRegion;

				ViewportRenderer->Resize(
					(size_t)contentRegion.x,
					(size_t)contentRegion.y);

				ImGuizmo::SetRect(
					ViewportPos.x,
					ViewportPos.y,
					contentRegion.x,
					contentRegion.y);
			}

			const auto& image = ViewportRenderer->GetFinalSceneTexture();
			if (image)
			{
				ImGui::Image(image->GetImGuiImage(), contentRegion);
			}
		}

		ImGui::End();
	}

	bool IsHoveringSceneViewport()
	{
		ImVec2 mousePos = ImGui::GetMousePos();
		return mousePos.x >= SceneViewportPos.x &&
			mousePos.x <= SceneViewportPos.x + SceneViewportSize.x &&
			mousePos.y >= SceneViewportPos.y &&
			mousePos.y <= SceneViewportPos.y + SceneViewportSize.y;
	}

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
		auto folderTextureAsset =
			MainAssetManager.GetAsset<TextureAsset>("folder_icon.png");

		auto fileTextureAsset =
			MainAssetManager.GetAsset<TextureAsset>("file_icon.png");

		_BrowserPanel.LoadTextures(
			folderTextureAsset->GetTexture(),
			fileTextureAsset->GetTexture());

		_SceneHierarchy.SetContext(CurrentScene->GetScene());

		SavedScene = std::make_shared<Scene>();

		FreeCam.ViewportWidth = MainViewport->GetWidth();
		FreeCam.ViewportHeight = MainViewport->GetHeight();
		FreeCam.SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
		FreeCam.CalculateProj();
		FreeCam.Active = true;

		MainViewport->GetResizeEvent().AddListener(
			[this](int width, int height)
			{
				_RenderContext->OnResize(width, height);
				ImGuiRenderer->Resize(width, height);
			});

		SceneViewportRenderer = std::make_shared<DeferredRenderer>(_RenderContext, (uint32_t)SceneViewportSize.x, (uint32_t)SceneViewportSize.y);
		ViewportRenderer = std::make_shared<DeferredRenderer>(_RenderContext, (uint32_t)ViewportSize.x, (uint32_t)ViewportSize.y);
		ImGuiRenderer = std::make_shared<DebugGUIRenderer>(_RenderContext, MainViewport);

		const auto& cubeMapAsset = MainAssetManager.GetAsset<CubeMapAsset>("sky.hycube");
		cubeMapAsset->Parse(cubeMapAsset->GetPath());
		Skybox = cubeMapAsset->GetCubeMap();

		_BrowserPanel.Setup();
	}

	virtual void OnShutdown() override
	{
		SceneViewportRenderer = nullptr;
		ViewportRenderer = nullptr;
		ImGuiRenderer = nullptr;
	}

	virtual void OnUpdate(float deltaTime) override
	{
		m_FPS = 1.0f / deltaTime;

		CurrentScene->GetScene()->RenderPhysicsDebug();

		if (m_IsSimulating)
			PhysicsUpdate(deltaTime);

		Entity cameraEntity;
		if (ViewportVisible && UpdateCamera(CurrentScene->GetScene(), cameraEntity, ViewportSize.x, ViewportSize.y))
		{
			const auto& camPos = cameraEntity.GetComponent<TransformComponent>().GetPosition();
			ViewportRenderer->Render(CurrentScene->GetScene(), cameraEntity.GetComponent<CameraComponent>(), camPos, {}, Skybox);
			//ViewportPostProcessing.PostProcessOffscreen(ViewportRenderer, ViewportSize.x, ViewportSize.y);
		}

		if (Input::IsMouseButtonDown(KeyCode::MouseRight) && IsHoveringSceneViewport())
		{
			Viewport::ConfineCursor(
				SceneViewportPos.x,
				SceneViewportPos.x + SceneViewportSize.x,
				SceneViewportPos.y,
				SceneViewportPos.y + SceneViewportSize.y);
			FreeCam.Update(deltaTime);
		}
		else
		{
			Viewport::ReleaseCursor();
		}

		if (SceneViewportVisible)
		{
			FreeCam.CalculateView();
			UpdateCameraViewportSize(FreeCam, {
					(int)SceneViewportRenderer->GetWidth(),
					(int)SceneViewportRenderer->GetHeight()
				});

			std::vector<Gizmo> gizmos;
			CurrentScene->GetScene()->IterateComponents(
				[&](Entity entity)
				{
					if (entity.HasComponent<CameraComponent>())
					{
						gizmos.push_back({ MainAssetManager.GetAsset<TextureAsset>("camera.png")->GetTexture(), entity.GetComponent<TransformComponent>().GetPosition(), {1, 1}});
					}
					else if (entity.HasComponent<PointLightComponent>())
					{
						gizmos.push_back({ MainAssetManager.GetAsset<TextureAsset>("point_light.png")->GetTexture(), entity.GetComponent<TransformComponent>().GetPosition(), {1, 1} });
					}
					else if (entity.HasComponent<DirectionalLightComponent>())
					{
						gizmos.push_back({ MainAssetManager.GetAsset<TextureAsset>("directional_light.png")->GetTexture(), entity.GetComponent<TransformComponent>().GetPosition(), {1, 1} });
					}
				});

			SceneViewportRenderer->Render(CurrentScene->GetScene(), FreeCam, FreeCam.GetPosition(), gizmos, Skybox);
			//SceneViewportPostProcessing.PostProcessOffscreen(SceneViewportRenderer, SceneViewportSize.x, SceneViewportSize.y);
		}

		RenderImGui(ImGuiRenderer);
		SubmitImGui(ImGuiRenderer);
	}

	virtual void OnImGuiRender() override
	{
		DrawViewports();
		DrawLogMessages();

		ImGui::Begin("Stats");
		ImGui::Text("FPS: %.1f", m_FPS);
		ImGui::End();

		DrawSettingsWindow();

		_BrowserPanel.OnImGuiRender();
		_EditorPanel.OnImGuiRender();
		_SceneHierarchy.OnImGuiRender();

		_Inspector.SetContext(
			CurrentScene->GetScene(),
			_SceneHierarchy.GetSelectedEntity());

		_Inspector.OnImGuiRender();

		RenderToolbar();
	}

	virtual void OnImGuiMenuBarRender() override
	{
		if (!ImGui::BeginMenuBar())
			return;

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Scene"))
				CurrentScene->Save();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Game"))
		{
			if (ImGui::MenuItem(m_IsSimulating ? "Stop" : "Play"))
				ToggleSimulation();

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	void RenderToolbar()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(
			ImVec2(ViewportPos.x, ViewportPos.y + 20));

		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags window_flags =
			ImGuiWindowFlags_NoDocking
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoSavedSettings;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::Begin("TOOLBAR", nullptr, window_flags);
		ImGui::PopStyleVar();

		if (ImGui::Button(m_IsSimulating ? "Stop" : "Play"))
			ToggleSimulation();

		ImGui::SameLine();

		if (ImGui::RadioButton("Translate",
			GuizmoTool == ImGuizmo::TRANSLATE))
			GuizmoTool = ImGuizmo::TRANSLATE;

		ImGui::SameLine();

		if (ImGui::RadioButton("Rotate",
			GuizmoTool == ImGuizmo::ROTATE))
			GuizmoTool = ImGuizmo::ROTATE;

		ImGui::SameLine();

		if (ImGui::RadioButton("Scale",
			GuizmoTool == ImGuizmo::SCALE))
			GuizmoTool = ImGuizmo::SCALE;

		ImGui::End();
	}
};

extern std::shared_ptr<Application> GetApplication()
{
	return std::make_shared<EditorApp>();
}
