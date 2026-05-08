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

static ImVec2 SceneViewportSize, SceneViewportPos;
static ImVec2 ViewportSize, ViewportPos;
static ImGuizmo::OPERATION GuizmoTool = ImGuizmo::TRANSLATE;

std::shared_ptr<Scene> SavedScene;

class EditorApp : public Application
{
private:

	float m_FPS = 0.0f;
	bool  m_IsSimulating = false;

	std::shared_ptr<Texture>     SceneViewportTexture;
	std::shared_ptr<Framebuffer> SceneViewportFramebuffer;
	std::shared_ptr<RenderPass>  SceneViewportRenderPass;

	std::shared_ptr<Texture>     ViewportTexture;
	std::shared_ptr<Framebuffer> ViewportFramebuffer;
	std::shared_ptr<RenderPass>  ViewportRenderPass;

	std::shared_ptr<RenderPass>  ImGuiRenderPass;
	std::shared_ptr<Framebuffer> ImGuiFramebuffer;
	std::shared_ptr<DebugGUI>    DebugGUI;

	std::shared_ptr<Pipeline> DefaultPipeline;

	std::shared_ptr<Renderer> MainRenderer;
	std::shared_ptr<Renderer> ImGuiRenderer;

	FreeCamera FreeCam;

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

	bool UpdateCamera(float deltaTime, Entity& outEntity)
	{
		auto scene = CurrentScene->GetScene();
		Entity activeCameraEntity;

		scene->IterateComponents<CameraComponent>(
			[&](Entity entity, CameraComponent& camera)
			{
				if (camera.Active)
					activeCameraEntity = entity;
			});

		glm::ivec2 size = {
				(int)ViewportTexture->GetWidth(),
				(int)ViewportTexture->GetHeight()
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
		ImGui::Begin("Scene Viewport");

		ImVec2 contentRegion = ImGui::GetContentRegionAvail();
		SceneViewportPos = ImGui::GetWindowPos();

		if (contentRegion.x != SceneViewportSize.x ||
			contentRegion.y != SceneViewportSize.y)
		{
			SceneViewportSize = contentRegion;

			SceneViewportTexture->Resize(
				(size_t)contentRegion.x,
				(size_t)contentRegion.y);

			SceneViewportFramebuffer->OnResize(
				(int)contentRegion.x,
				(int)contentRegion.y);

			ImGuizmo::SetRect(
				SceneViewportPos.x,
				SceneViewportPos.y,
				contentRegion.x,
				contentRegion.y);
		}

		ImGui::Image(SceneViewportTexture->GetImGuiImage(), contentRegion);

		DrawGizmo();

		ImGui::End();

		ImGui::Begin("Game Viewport");

		ViewportPos = ImGui::GetWindowPos();

		if (contentRegion.x != ViewportSize.x ||
			contentRegion.y != ViewportSize.y)
		{
			ViewportSize = contentRegion;

			ViewportTexture->Resize(
				(size_t)contentRegion.x,
				(size_t)contentRegion.y);

			ViewportFramebuffer->OnResize(
				(int)contentRegion.x,
				(int)contentRegion.y);

			ImGuizmo::SetRect(
				ViewportPos.x,
				ViewportPos.y,
				contentRegion.x,
				contentRegion.y);
		}

		ImGui::Image(ViewportTexture->GetImGuiImage(), contentRegion);

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
		FreeCam.CalculateProj();
		FreeCam.Active = true;

		SceneViewportTexture =
			Texture::Create(_RenderContext,
				TextureFormat::ViewportDefault,
				1920, 1080);

		SceneViewportRenderPass =
			RenderPass::Create(_RenderContext, SceneViewportTexture);

		SceneViewportFramebuffer =
			Framebuffer::Create(_RenderContext,
				SceneViewportRenderPass,
				SceneViewportTexture);

		ViewportTexture =
			Texture::Create(_RenderContext,
				TextureFormat::ViewportDefault,
				1920, 1080);

		ViewportRenderPass =
			RenderPass::Create(_RenderContext, ViewportTexture);

		ViewportFramebuffer =
			Framebuffer::Create(_RenderContext,
				ViewportRenderPass,
				ViewportTexture);

		ImGuiRenderPass = RenderPass::Create(_RenderContext);
		DebugGUI = DebugGUI::Create(_RenderContext, ImGuiRenderPass);
		ImGuiFramebuffer =
			Framebuffer::Create(_RenderContext, ImGuiRenderPass);

		MainViewport->GetResizeEvent().AddListener(
			[this](int width, int height)
			{
				_RenderContext->OnResize(width, height);
				ImGuiFramebuffer->OnResize(width, height);
			});

		MainRenderer = std::make_shared<Renderer>(_RenderContext);
		ImGuiRenderer = std::make_shared<Renderer>(_RenderContext);

		MainRenderer->CreateDebugPipelines(
			ImGuiRenderPass,
			MainAssetManager.GetAsset<ShaderAsset>("DebugLineVertexShader.glsl"),
			MainAssetManager.GetAsset<ShaderAsset>("DebugLineFragmentShader.glsl"));

		DefaultPipeline =
			MainRenderer->CreatePipeline(
				ViewportRenderPass,
				MainAssetManager.GetAsset<ShaderAsset>("VertexShader.glsl"),
				MainAssetManager.GetAsset<ShaderAsset>("FragmentShader.glsl"));
	}

	virtual void OnShutdown() override {}

	virtual void OnUpdate(float deltaTime) override
	{
		m_FPS = 1.0f / deltaTime;

		CurrentScene->GetScene()->RenderPhysicsDebug();

		if (m_IsSimulating)
			PhysicsUpdate(deltaTime);

		Entity cameraEntity;
		if (UpdateCamera(deltaTime, cameraEntity))
		{
			Render(deltaTime,
				MainRenderer,
				DefaultPipeline,
				ViewportFramebuffer,
				ViewportRenderPass,
				cameraEntity.GetComponent<CameraComponent>(),
				cameraEntity.GetComponent<TransformComponent>().GetPosition());
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

		FreeCam.CalculateView();
		UpdateCameraViewportSize(FreeCam, {
				(int)SceneViewportTexture->GetWidth(),
				(int)SceneViewportTexture->GetHeight()
			});

		Render(
			deltaTime,
			MainRenderer,
			DefaultPipeline,
			SceneViewportFramebuffer,
			SceneViewportRenderPass,
			FreeCam,
			FreeCam.GetPosition()
		);

		RenderImGui(DebugGUI);
		SubmitImGui(DebugGUI,
			ImGuiRenderer,
			ImGuiFramebuffer,
			ImGuiRenderPass);
	}

	virtual void OnImGuiRender() override
	{
		ImGui::Begin("Stats");
		ImGui::Text("FPS: %.1f", m_FPS);
		ImGui::End();

		DrawViewports();
		DrawLogMessages();

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
