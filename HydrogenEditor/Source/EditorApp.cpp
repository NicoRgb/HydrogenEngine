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
static ImVec2 ViewportSize;

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
		ImVec2 windowPos = ImGui::GetWindowPos();
		if (contentRegion.x != ViewportSize.x || contentRegion.y != ViewportSize.y)
		{
			ViewportSize = contentRegion;

			ViewportTexture->Resize((size_t)contentRegion.x, (size_t)contentRegion.y);
			ViewportFramebuffer->OnResize((int)contentRegion.x, (int)contentRegion.y);

			ImGuizmo::SetRect(windowPos.x, windowPos.y, contentRegion.x, contentRegion.y);
		}

		ImGui::Image(ViewportTexture->GetImGuiImage(), contentRegion);

		auto selectedEntity = _SceneHierarchy.GetSelectedEntity();
		if (selectedEntity.IsValid())
		{
			ImGuizmo::SetDrawlist();

			auto& tc = selectedEntity.GetComponent<Hydrogen::TransformComponent>();
			glm::mat4 transform = tc.Transform;

			ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
			ImGuizmo::MODE mode = ImGuizmo::WORLD;

			if (Hydrogen::Input::IsKeyDown(Hydrogen::KeyCode::W)) operation = ImGuizmo::TRANSLATE;
			if (Hydrogen::Input::IsKeyDown(Hydrogen::KeyCode::E)) operation = ImGuizmo::ROTATE;
			if (Hydrogen::Input::IsKeyDown(Hydrogen::KeyCode::R)) operation = ImGuizmo::SCALE;

			glm::mat4 view = FreeCam.View;
			glm::mat4 proj = FreeCam.Proj;
			proj[1][1] *= -1;

			ImGuizmo::Manipulate(
				glm::value_ptr(view),
				glm::value_ptr(proj),
				operation,
				mode,
				glm::value_ptr(transform)
			);

			if (ImGuizmo::IsUsing())
			{
				tc.Transform = transform;
			}
		}
		ImGui::End();

		_BrowserPanel.OnImGuiRender();
		_EditorPanel.OnImGuiRender();
		_SceneHierarchy.OnImGuiRender();

		_Inspector.SetContext(CurrentScene->GetScene(), _SceneHierarchy.GetSelectedEntity());
		_Inspector.OnImGuiRender();
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
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<EditorApp>();
}
