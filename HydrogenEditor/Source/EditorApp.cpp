#include <Hydrogen/HydrogenMain.hpp>

#include "AssetBrowserPanel.hpp"
#include "AssetEditorPanel.hpp"
#include "InspectorPanel.hpp"
#include "SceneHierarchyPanel.hpp"
#include "ImGuizmo.h"

static AssetEditorPanel _EditorPanel;
static AssetBrowserPanel _BrowserPanel("assets", _EditorPanel);
static SceneHierarchyPanel _SceneHierarchy;
static InspectorPanel _Inspector;
static ImVec2 ViewportSize, ViewportPos;
ImGuizmo::OPERATION GuizmoTool = ImGuizmo::TRANSLATE;

std::shared_ptr<Hydrogen::Scene> SavedScene;

class EditorApp : public Hydrogen::Application
{
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
		auto folderTextureAsset = MainAssetManager.GetAsset<Hydrogen::TextureAsset>("folder_icon.png");
		auto fileTextureAsset = MainAssetManager.GetAsset<Hydrogen::TextureAsset>("file_icon.png");

		_BrowserPanel.LoadTextures(folderTextureAsset->GetTexture(), fileTextureAsset->GetTexture());
		_SceneHierarchy.SetContext(CurrentScene->GetScene());

		SavedScene = std::make_shared<Hydrogen::Scene>();

		FreeCam.Active = true;
	}

	virtual void OnShutdown() override
	{
	}

	virtual void OnUpdate() override
	{
	}

	virtual void OnImGuiRender() override
	{
		ImGui::Begin("Viewport");
		ImVec2 contentRegion = ImGui::GetContentRegionAvail();
		ViewportPos = ImGui::GetWindowPos();
		if (contentRegion.x != ViewportSize.x || contentRegion.y != ViewportSize.y)
		{
			ViewportSize = contentRegion;

			ViewportTexture->Resize((size_t)contentRegion.x, (size_t)contentRegion.y);
			ViewportFramebuffer->OnResize((int)contentRegion.x, (int)contentRegion.y);

			ImGuizmo::SetRect(ViewportPos.x, ViewportPos.y, contentRegion.x, contentRegion.y);
		}

		ImGui::Image(ViewportTexture->GetImGuiImage(), contentRegion);

		auto selectedEntity = _SceneHierarchy.GetSelectedEntity();
		if (selectedEntity.IsValid() && FreeCam.Active)
		{
			ImGuizmo::SetDrawlist();

			auto& tc = selectedEntity.GetComponent<Hydrogen::TransformComponent>();
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
			{
				tc.Transform = transform;
			}
		}
		ImGui::End();

		ImGui::Begin("Log");

		ImGui::BeginChild("LogScrollRegion");

		for (auto& m : Hydrogen::EngineLogger::GetLogger()->GetLogSink()->GetMessages())
		{
			ImVec4 color;
			switch (m.level)
			{
			case spdlog::level::trace: color = ImVec4(1, 1, 1, 1); break;
			case spdlog::level::debug: color = ImVec4(0.6f, 0.6f, 1, 1); break;
			case spdlog::level::info:  color = ImVec4(1, 1, 1, 1); break;
			case spdlog::level::warn:  color = ImVec4(1, 1, 0.1f, 1); break;
			case spdlog::level::err:   color = ImVec4(1, 0.3f, 0.3f, 1); break;
			case spdlog::level::critical: color = ImVec4(1, 0, 0, 1); break;
			}

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(m.message.c_str());
			ImGui::PopStyleColor();
		}

		for (auto& m : Hydrogen::AppLogger::GetLogger()->GetLogSink()->GetMessages())
		{
			ImVec4 color;
			switch (m.level)
			{
			case spdlog::level::trace: color = ImVec4(1, 1, 1, 1); break;
			case spdlog::level::debug: color = ImVec4(0.6f, 0.6f, 1, 1); break;
			case spdlog::level::info:  color = ImVec4(1, 1, 1, 1); break;
			case spdlog::level::warn:  color = ImVec4(1, 1, 0.1f, 1); break;
			case spdlog::level::err:   color = ImVec4(1, 0.3f, 0.3f, 1); break;
			case spdlog::level::critical: color = ImVec4(1, 0, 0, 1); break;
			}

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(m.message.c_str());
			ImGui::PopStyleColor();
		}

		ImGui::EndChild();

		ImGui::End();

		_BrowserPanel.OnImGuiRender();
		_EditorPanel.OnImGuiRender();
		_SceneHierarchy.OnImGuiRender();

		_Inspector.SetContext(CurrentScene->GetScene(), _SceneHierarchy.GetSelectedEntity());
		_Inspector.OnImGuiRender();

		RenderToolbar();
	}

	virtual void OnImGuiMenuBarRender() override
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Save Scene"))
				{
					CurrentScene->Save();
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Game"))
			{
				if (ImGui::MenuItem(FreeCam.Active ? "Play" : "Stop"))
				{
					FreeCam.Active = !FreeCam.Active;
					if (FreeCam.Active)
					{
						SavedScene = std::make_shared<Hydrogen::Scene>();
						SavedScene->DeserializeScene(CurrentScene->GetScene()->SerializeScene(), &MainAssetManager);
						CurrentScene->GetScene()->CreateScripts();
					}
					else
					{
						CurrentScene->ClearScene();
						CurrentScene->GetScene()->DeserializeScene(SavedScene->SerializeScene(), &MainAssetManager);
					}
					_SceneHierarchy.SetContext(CurrentScene->GetScene());
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
	}

	void RenderToolbar()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(ImVec2(ViewportPos.x, ViewportPos.y+20));
		//ImGui::SetNextWindowSize(ImVec2(ViewportSize.x / 2, 50));
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags window_flags = 0
			| ImGuiWindowFlags_NoDocking
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoSavedSettings;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::Begin("TOOLBAR", NULL, window_flags);
		ImGui::PopStyleVar();

		if (ImGui::Button(FreeCam.Active ? "Play" : "Stop"))
		{
			if (FreeCam.Active)
			{
				SavedScene = std::make_shared<Hydrogen::Scene>();
				SavedScene->DeserializeScene(CurrentScene->GetScene()->SerializeScene(), &MainAssetManager);
				CurrentScene->GetScene()->CreateScripts();
			}
			else
			{
				CurrentScene->ClearScene();
				CurrentScene->GetScene()->DeserializeScene(SavedScene->SerializeScene(), &MainAssetManager);
			}

			FreeCam.Active = !FreeCam.Active;
			_SceneHierarchy.SetContext(CurrentScene->GetScene());
		}

		ImGui::SameLine();

		if (ImGui::RadioButton("Translate", GuizmoTool == ImGuizmo::TRANSLATE))
			GuizmoTool = ImGuizmo::TRANSLATE;

		ImGui::SameLine();

		if (ImGui::RadioButton("Rotate", GuizmoTool == ImGuizmo::ROTATE))
			GuizmoTool = ImGuizmo::ROTATE;

		ImGui::SameLine();

		if (ImGui::RadioButton("Scale", GuizmoTool == ImGuizmo::SCALE))
			GuizmoTool = ImGuizmo::SCALE;

		ImGui::End();
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<EditorApp>();
}
