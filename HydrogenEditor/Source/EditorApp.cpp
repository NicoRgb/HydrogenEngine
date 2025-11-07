#include <Hydrogen/HydrogenMain.hpp>

#include "AssetBrowserPanel.hpp"
#include "AssetEditorPanel.hpp"
#include "InspectorPanel.hpp"
#include "SceneHierarchyPanel.hpp"

static AssetEditorPanel _EditorPanel;
static AssetBrowserPanel _BrowserPanel("assets", _EditorPanel);
static SceneHierarchyPanel _SceneHierarchy;
static InspectorPanel _Inspector;

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
		_SceneHierarchy.SetContext(CurrentScene);
	}

	virtual void OnShutdown() override
	{
	}

	virtual void OnUpdate() override
	{
	}

	virtual void OnImGuiRender() override
	{
		_BrowserPanel.OnImGuiRender();
		_EditorPanel.OnImGuiRender();
		_SceneHierarchy.OnImGuiRender();

		_Inspector.SetContext(CurrentScene, _SceneHierarchy.GetSelectedEntity());
		_Inspector.OnImGuiRender();
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<EditorApp>();
}
