#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <filesystem>
#include <json.hpp>

#include "GUISystem.hpp"

#include <Hydrogen/Camera.hpp>
#include <Hydrogen/Renderer/RenderBuffer.hpp>

class AssetInspectorPanel : public EditorPanel
{
public:
    virtual void OnAttach();
    virtual void OnDetach();

    virtual void OnUpdate(float deltaTime) {}
    virtual void OnImGuiRender();

    virtual std::string GetTitle() const { return "Asset Inspector"; }

    virtual DockDirection GetDefaultDockDirection() const { return DockDirection::Right_Top; }
    virtual float GetDefaultDockSplitRatio() const { return 0.25f; }

private:
    void DrawFileConfig(std::filesystem::path path, nlohmann::json& j);

    std::filesystem::path m_CurrentFile;

    std::unique_ptr<Hydrogen::Renderer> m_MaterialPreviewRenderer;
    std::shared_ptr<Hydrogen::SceneAsset> m_MaterialPreviewScene;
};
