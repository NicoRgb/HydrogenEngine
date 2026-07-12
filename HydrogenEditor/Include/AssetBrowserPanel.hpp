#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <filesystem>
#include <json.hpp>

#include "Panel.hpp"
#include "AssetEditorPanel.hpp"

#include <Hydrogen/Camera.hpp>
#include <Hydrogen/Renderer/RenderBuffer.hpp>

class AssetBrowserPanel : public Panel
{
public:
    AssetBrowserPanel(const std::filesystem::path& assetDir, AssetEditorPanel& editor);

    void Setup();

    void LoadTextures(const Hydrogen::Texture* folderTex, const Hydrogen::Texture* fileTex);
    void OnImGuiRender() override;
    const char* GetName() const override { return "Asset Browser"; }

private:
    void DrawFileConfig(std::filesystem::path path, nlohmann::json& j);
    void Render(const Hydrogen::CameraComponent& camera, const glm::vec3 cameraPos);

    AssetEditorPanel& m_EditorPanel;
    std::filesystem::path m_AssetDirectory, m_CurrentDirectory, m_CurrentFile;
    const Hydrogen::Texture* m_FolderTexture;
    const Hydrogen::Texture* m_FileTexture;

    bool showCreateFolderDialog = false, showCreateFileDialog = false;
    char inputName[256] = "";

    std::unique_ptr<Hydrogen::Renderer> m_MaterialPreviewRenderer;
    std::unique_ptr<Hydrogen::RenderBuffer> m_UniformBuffer;
    std::shared_ptr<Hydrogen::Scene> m_MaterialPreviewScene;

    VkImageView m_FinalImage;
};
