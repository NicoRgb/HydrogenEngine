#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <filesystem>
#include <json.hpp>

#include "Panel.hpp"
#include "AssetEditorPanel.hpp"

class AssetBrowserPanel : public Panel
{
public:
    AssetBrowserPanel(const std::filesystem::path& assetDir, AssetEditorPanel& editor);

    void LoadTextures(std::shared_ptr<Hydrogen::Texture> folderTex, std::shared_ptr<Hydrogen::Texture> fileTex);
    void OnImGuiRender() override;
    const char* GetName() const override { return "Asset Browser"; }

private:
    void DrawFileConfig(nlohmann::json& j);

private:
    AssetEditorPanel& m_EditorPanel;
    std::filesystem::path m_AssetDirectory, m_CurrentDirectory, m_CurrentFile;
    std::shared_ptr<Hydrogen::Texture> m_FolderTexture, m_FileTexture;

    bool showCreateFolderDialog = false, showCreateFileDialog = false;
    char inputName[256] = "";
};
