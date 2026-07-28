#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <filesystem>
#include <json.hpp>

#include "GUISystem.hpp"

#include <Hydrogen/Scene/Camera.hpp>
#include <Hydrogen/Renderer/RenderBuffer.hpp>

class AssetBrowserPanel : public EditorPanel
{
public:
	void OnAttach() override;
	void OnDetach() override;

	virtual void OnUpdate(float deltaTime) override {}
	void OnImGuiRender() override;

	virtual std::string GetTitle() const { return "Asset Browser"; }

	virtual DockDirection GetDefaultDockDirection() const { return DockDirection::Bottom; }
	virtual float GetDefaultDockSplitRatio() const { return 0.25f; }

private:
	void RenderDirectoryTree(const std::filesystem::path& directoryPath);
	void RenderBreadcrumbs();
	void StartRenaming(const std::filesystem::path& path);

	std::filesystem::path m_AssetDirectory;
	std::filesystem::path m_CurrentDirectory;

	float m_ThumbnailSize = 50.0f;

	std::filesystem::path m_RenamingPath;
	char m_RenameBuffer[256] = "";
};
