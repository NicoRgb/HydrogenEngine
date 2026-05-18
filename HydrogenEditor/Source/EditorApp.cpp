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
static ImVec2 ViewportSize = { 500, 500 };
static ImVec2 ViewportPos = { 0, 0 };
static ImVec2 MaterialPreviewSize = { 500, 500 };
static ImVec2 MaterialPreviewPos = { 0, 0 };
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

	PostProcessing SceneViewportPostProcessing;
	PostProcessing ViewportPostProcessing;

	std::shared_ptr<DeferredRenderer> MaterialPreviewRenderer;
	std::shared_ptr<Scene> MaterialPreviewScene;
	std::shared_ptr<MaterialAsset> PreviewMaterial;
	PostProcessing MaterialPreviewPP;

	std::shared_ptr<DebugGUI> DebugGUI;
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

			SceneViewportRenderer->Resize(
				(size_t)contentRegion.x,
				(size_t)contentRegion.y);

			ImGuizmo::SetRect(
				SceneViewportPos.x,
				SceneViewportPos.y,
				contentRegion.x,
				contentRegion.y);
		}

		ImGui::Image(SceneViewportPostProcessing.GetFinalImage()->GetImGuiImage(), contentRegion);

		DrawGizmo();

		ImGui::End();

		ImGui::Begin("Game Viewport");

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

		ImGui::Image(ViewportPostProcessing.GetFinalImage()->GetImGuiImage(), contentRegion);

		ImGui::End();

		ImGui::Begin("Material Preview");

		MaterialPreviewPos = ImGui::GetWindowPos();

		if (contentRegion.x != MaterialPreviewSize.x ||
			contentRegion.y != MaterialPreviewSize.y)
		{
			MaterialPreviewSize = contentRegion;

			MaterialPreviewRenderer->Resize(
				(size_t)contentRegion.x,
				(size_t)contentRegion.y);

			ImGuizmo::SetRect(
				MaterialPreviewPos.x,
				MaterialPreviewPos.y,
				contentRegion.x,
				contentRegion.y);
		}

		ImGui::Image(MaterialPreviewPP.GetFinalImage()->GetImGuiImage(), contentRegion);

		ImGui::End();

		ImGui::Begin("Material Settings");

		float metallic = PreviewMaterial->GetMetallicFactor();
		if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f))
			PreviewMaterial->SetMetallicFactor(metallic);

		float roughness = PreviewMaterial->GetRoughnessFactor();
		if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f))
			PreviewMaterial->SetRoughnessFactor(roughness);

		glm::vec3 tint = PreviewMaterial->GetTint();
		if (ImGui::ColorPicker3("Tint", glm::value_ptr(tint)))
			PreviewMaterial->SetTint(tint);

		glm::vec3 emissive = PreviewMaterial->GetEmissive();
		float emissiveStrength = PreviewMaterial->GetEmissive().a;
		if (ImGui::ColorPicker3("Emissive", glm::value_ptr(emissive)) || ImGui::SliderFloat("Emissive Strength", &emissiveStrength, 0.0f, 10.0f))
			PreviewMaterial->SetEmissive(glm::vec4(emissive, emissiveStrength));

		auto albedo = PreviewMaterial->GetAlbedoMap();
		auto normal = PreviewMaterial->GetNormalMap();
		auto orm = PreviewMaterial->GetORMMap();

		ImGui::Separator();

		ImGui::Text("Albedo Map");
		if (albedo)
		{
			ImGui::Text(albedo->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(newPath.filename().string());
				if (asset)
				{
					PreviewMaterial->SetAlbedoMap(asset);
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::Text("Normal Map");
		if (normal)
		{
			ImGui::Text(normal->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(newPath.filename().string());
				if (asset)
				{
					PreviewMaterial->SetNormalMap(asset);
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::Text("ORM Map");
		if (orm)
		{
			ImGui::Text(orm->GetPath().c_str());
		}
		else
		{
			ImGui::Text("NULL");
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
			{
				std::filesystem::path newPath((const char*)payload->Data);
				auto asset = Application::Get()->MainAssetManager.GetAsset<TextureAsset>(newPath.filename().string());
				if (asset)
				{
					PreviewMaterial->SetORMMap(asset);
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::Separator();
		ImGui::Text("Light Settings");

		MaterialPreviewScene->IterateComponents<PointLightComponent>(
			[&](Entity entity, PointLightComponent& l)
			{
				TransformComponent::OnImGuiRender(entity.GetComponent<TransformComponent>());
				PointLightComponent::OnImGuiRender(l);
			});

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

		MainViewport->GetResizeEvent().AddListener(
			[this](int width, int height)
			{
				_RenderContext->OnResize(width, height);
				ImGuiRenderer->Resize(width, height);
			});

		SceneViewportRenderer = std::make_shared<DeferredRenderer>(_RenderContext, (uint32_t)SceneViewportSize.x, (uint32_t)SceneViewportSize.y);
		ViewportRenderer = std::make_shared<DeferredRenderer>(_RenderContext, (uint32_t)ViewportSize.x, (uint32_t)ViewportSize.y);
		ImGuiRenderer = std::make_shared<DebugGUIRenderer>(_RenderContext, MainViewport);
		MaterialPreviewRenderer = std::make_shared<DeferredRenderer>(_RenderContext, (uint32_t)MaterialPreviewSize.x, (uint32_t)MaterialPreviewSize.y);

		auto s = MainAssetManager.GetAsset<SceneAsset>("MaterialPreview.hyscene");
		s->Load(&MainAssetManager);
		MaterialPreviewScene = s->GetScene();

		MaterialPreviewScene->IterateComponents<MeshRendererComponent>(
			[&](Entity entity, MeshRendererComponent& r)
			{
				PreviewMaterial = r.Material;
			});
	}

	virtual void OnShutdown() override {}

	virtual void OnUpdate(float deltaTime) override
	{
		m_FPS = 1.0f / deltaTime;

		CurrentScene->GetScene()->RenderPhysicsDebug();

		if (m_IsSimulating)
			PhysicsUpdate(deltaTime);

		Entity cameraEntity;
		if (UpdateCamera(CurrentScene->GetScene(), cameraEntity, ViewportSize.x, ViewportSize.y))
		{
			const auto& camPos = cameraEntity.GetComponent<TransformComponent>().GetPosition();
			ViewportRenderer->Render(CurrentScene->GetScene(), cameraEntity.GetComponent<CameraComponent>(), camPos);
			ViewportPostProcessing.PostProcessOffscreen(ViewportRenderer, ViewportSize.x, ViewportSize.y);
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
				(int)SceneViewportRenderer->GetWidth(),
				(int)SceneViewportRenderer->GetHeight()
			});

		SceneViewportRenderer->Render(CurrentScene->GetScene(), FreeCam, FreeCam.GetPosition());
		if (cameraEntity.IsValid())
		{
			const auto& camPos = cameraEntity.GetComponent<TransformComponent>().GetPosition();
			SceneViewportRenderer->RenderGizmos({ { MainAssetManager.GetAsset<TextureAsset>("statue.jpg")->GetTexture(), camPos, {1, 1}} }, FreeCam, FreeCam.GetPosition());
		}
		SceneViewportPostProcessing.PostProcessOffscreen(SceneViewportRenderer, SceneViewportSize.x, SceneViewportSize.y);

		if (UpdateCamera(MaterialPreviewScene, cameraEntity, MaterialPreviewSize.x, MaterialPreviewSize.y))
		{
			MaterialPreviewRenderer->Render(MaterialPreviewScene, cameraEntity.GetComponent<CameraComponent>(), cameraEntity.GetComponent<TransformComponent>().GetPosition());
			MaterialPreviewPP.PostProcessOffscreen(MaterialPreviewRenderer, MaterialPreviewSize.x, MaterialPreviewSize.y);
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
