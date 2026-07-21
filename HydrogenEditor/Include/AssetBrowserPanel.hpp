#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <filesystem>
#include <json.hpp>

#include "GUISystem.hpp"

#include <Hydrogen/Camera.hpp>
#include <Hydrogen/Renderer/RenderBuffer.hpp>

class AssetBrowserPanel : public EditorPanel
{
public:
    virtual void OnAttach();
    virtual void OnDetach();

    virtual void OnUpdate(float deltaTime) {}
    virtual void OnImGuiRender();

    virtual std::string GetTitle() const { return "Asset Browser"; }

    virtual DockDirection GetDefaultDockDirection() const { return DockDirection::Bottom; }
    virtual float GetDefaultDockSplitRatio() const { return 0.25f; }

private:
    std::filesystem::path m_AssetDirectory, m_CurrentDirectory;

    bool showCreateFolderDialog = false, showCreateFileDialog = false;
    char inputName[256] = "";
};
