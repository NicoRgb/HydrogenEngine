#define IMGUI_DEFINE_MATH_OPERATORS
#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include "Panels/AssetBrowserPanel.hpp"
#include "Editors/AnimGraphEditor.hpp"
#include "Editors/CodeEditor.hpp"

using json = nlohmann::json;
using namespace Hydrogen;

extern ImGuiTextureCache TextureCache;
extern VkSampler ImguiSampler;

namespace
{
	static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
	{
		using namespace ImGui;
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;
		ImGuiID id = window->GetID("##Splitter");

		ImVec2 pos = window->DC.CursorPos;
		ImVec2 size = split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1);

		ImRect bb;
		bb.Min = ImVec2(pos.x + size.x, pos.y + size.y);

		ImVec2 axis_size = split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness);
		ImVec2 calc_size = CalcItemSize(axis_size, 0.0f, 0.0f);

		bb.Max = ImVec2(bb.Min.x + calc_size.x, bb.Min.y + calc_size.y);

		return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
	}

	ImTextureID GetIconTextureID(const std::string& iconFilename)
	{
		auto asset = Application::Get()->MainAssetManager.TryGetAsset<TextureAsset>(iconFilename);
		if (asset && asset->GetTexture(Application::Get()->GetRenderDevice()))
		{
			auto tex = asset->GetTexture(Application::Get()->GetRenderDevice());
			return TextureCache.GetTextureID(tex->GetImageView(), ImguiSampler);
		}

		auto defaultAsset = Application::Get()->MainAssetManager.GetAsset<TextureAsset>("icon_file.png");
		auto defaultTex = defaultAsset->GetTexture(Application::Get()->GetRenderDevice());
		return TextureCache.GetTextureID(defaultTex->GetImageView(), ImguiSampler);
	}

	std::string GetIconFilenameForExtension(const std::filesystem::path& path)
	{
		std::string ext = path.extension().string();
		if (ext == ".png" || ext == ".jpg" || ext == ".hdr" || ext == ".tga") return "icon_texture.png";
		if (ext == ".glsl" || ext == ".vert" || ext == ".frag")               return "icon_shader.png";
		if (ext == ".lua")                                                    return "icon_script.png";
		if (ext == ".hymaterial")                                             return "icon_material.png";
		if (ext == ".hyscene")                                                return "icon_scene.png";
		if (ext == ".hyanim")                                                 return "icon_animation.png";

		return "icon_file.png";
	}

	std::string TruncateFileName(const std::string& name, float thumbnailSize)
	{
		int maxChars = static_cast<int>(thumbnailSize / 7.0f);
		if (maxChars < 4) maxChars = 4;

		if (static_cast<int>(name.length()) > maxChars)
		{
			return name.substr(0, maxChars - 3) + "...";
		}
		return name;
	}
}

void AssetBrowserPanel::OnAttach()
{
	m_AssetDirectory = Application::Get()->MainAssetManager.GetAssetDirectory();
	m_CurrentDirectory = m_AssetDirectory;
}

void AssetBrowserPanel::OnDetach()
{
}

void AssetBrowserPanel::StartRenaming(const std::filesystem::path& path)
{
	m_RenamingPath = path;
	memset(m_RenameBuffer, 0, sizeof(m_RenameBuffer));
	std::string name = path.filename().string();
	strncpy_s(m_RenameBuffer, name.c_str(), sizeof(m_RenameBuffer) - 1);
}

void AssetBrowserPanel::RenderDirectoryTree(const std::filesystem::path& directoryPath)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	bool isCurrentOrParent = false;
	auto rel = std::filesystem::relative(m_CurrentDirectory, directoryPath);
	if (!rel.empty() && rel.native()[0] != '.')
	{
		isCurrentOrParent = true;
	}

	if (isCurrentOrParent)
		flags |= ImGuiTreeNodeFlags_DefaultOpen;

	if (directoryPath == m_CurrentDirectory)
		flags |= ImGuiTreeNodeFlags_Selected;

	std::string folderName = (directoryPath == m_AssetDirectory) ? "Assets" : directoryPath.filename().string();

	bool isOpened = ImGui::TreeNodeEx((void*)std::filesystem::hash_value(directoryPath), flags, "%s", folderName.c_str());

	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
	{
		m_CurrentDirectory = directoryPath;
	}

	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE"))
		{
			std::filesystem::path sourcePath((const char*)payload->Data);
			std::filesystem::path destPath = directoryPath / sourcePath.filename();

			std::error_code ec;
			std::filesystem::rename(sourcePath, destPath, ec);
		}
		ImGui::EndDragDropTarget();
	}

	if (isOpened)
	{
		for (auto& entry : std::filesystem::directory_iterator(directoryPath))
		{
			if (entry.path().extension() == ".hyasset") continue;

			if (entry.is_directory())
			{
				RenderDirectoryTree(entry.path());
			}
			else
			{
				ImGuiTreeNodeFlags fileFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
				ImGui::TreeNodeEx((void*)std::filesystem::hash_value(entry.path()), fileFlags, "%s", entry.path().filename().string().c_str());

				if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
				{
					std::string pathStr = entry.path().string();
					ImGui::SetDragDropPayload("ASSET_FILE", pathStr.c_str(), pathStr.size() + 1);
					ImGui::Text("%s", entry.path().filename().string().c_str());
					ImGui::EndDragDropSource();
				}

				if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::GetDragDropPayload())
				{
					Dockspace->GetEventBus().Publish(AssetSelectedEvent{ entry.path().string() });
				}
			}
		}
		ImGui::TreePop();
	}
}

void AssetBrowserPanel::RenderBreadcrumbs()
{
	std::vector<std::filesystem::path> pathParts;
	std::filesystem::path curr = m_CurrentDirectory;

	while (curr != m_AssetDirectory && curr.has_parent_path())
	{
		pathParts.push_back(curr);
		curr = curr.parent_path();
	}
	pathParts.push_back(m_AssetDirectory);

	std::reverse(pathParts.begin(), pathParts.end());

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 4));

	for (size_t i = 0; i < pathParts.size(); i++)
	{
		std::string partName = (pathParts[i] == m_AssetDirectory) ? "Assets" : pathParts[i].filename().string();

		if (ImGui::Button(partName.c_str()))
		{
			m_CurrentDirectory = pathParts[i];
		}

		if (i < pathParts.size() - 1)
		{
			ImGui::SameLine();
			ImGui::TextDisabled("/");
			ImGui::SameLine();
		}
	}

	ImGui::PopStyleVar();
}

void AssetBrowserPanel::OnImGuiRender()
{
	ImGui::SetNextItemWidth(100.0f);
	ImGui::SliderFloat("##IconSize", &m_ThumbnailSize, 32.0f, 128.0f, "%.0f px");

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	RenderBreadcrumbs();
	ImGui::Separator();

	static float treeViewWidth = 220.0f;
	static float gridViewWidth = 0.0f;
	Splitter(true, 4.0f, &treeViewWidth, &gridViewWidth, 80.0f, 100.0f);

	ImGui::BeginChild("FolderTreeHierarchy", ImVec2(treeViewWidth, 0), true);
	RenderDirectoryTree(m_AssetDirectory);
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("AssetGridView", ImVec2(0, 0), true);

	static float padding = 16.0f;
	float cellSize = m_ThumbnailSize + padding;
	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = static_cast<int>(panelWidth / cellSize);
	if (columnCount < 1) columnCount = 1;

	ImGui::Columns(columnCount, 0, false);

	ImTextureID folderIconID = GetIconTextureID("icon_folder.png");

	for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
	{
		const auto& path = entry.path();
		if (path.extension() == ".hyasset") continue;

		std::string filename = path.filename().string();
		ImGui::PushID(filename.c_str());
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));

		ImTextureID iconID = entry.is_directory() ? folderIconID : GetIconTextureID(GetIconFilenameForExtension(path));

		ImGui::ImageButton("[ITEM]", iconID, { m_ThumbnailSize, m_ThumbnailSize });

		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
		{
			std::string fullPathStr = path.string();

			if (entry.is_directory())
			{
				ImGui::SetDragDropPayload("ASSET_BROWSER_ITEM", fullPathStr.c_str(), fullPathStr.size() + 1);
			}
			else
			{
				ImGui::SetDragDropPayload("ASSET_FILE", fullPathStr.c_str(), fullPathStr.size() + 1);
			}

			ImGui::Text("%s", filename.c_str());
			ImGui::EndDragDropSource();
		}

		if (entry.is_directory() && ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_FILE");
			if (!payload)
			{
				payload = ImGui::AcceptDragDropPayload("ASSET_BROWSER_ITEM");
			}

			if (payload)
			{
				std::filesystem::path sourcePath((const char*)payload->Data);
				if (sourcePath != path)
				{
					std::filesystem::path destPath = path / sourcePath.filename();
					std::error_code ec;
					std::filesystem::rename(sourcePath, destPath, ec);
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (entry.is_directory())
			{
				m_CurrentDirectory /= path.filename();
			}
			else
			{
				if (path.extension() == ".lua" || path.extension() == ".glsl")
				{
					Dockspace->OpenDocument<CodeEditor>(path.string());
				}
				else if (path.extension() == ".hygraph")
				{
					Dockspace->OpenDocument<AnimGraphEditor>(path.string());
				}
			}
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && !ImGui::GetDragDropPayload() && !entry.is_directory())
		{
			Dockspace->GetEventBus().Publish(AssetSelectedEvent{ (m_CurrentDirectory / path.filename()).string() });
		}

		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Rename"))
			{
				StartRenaming(path);
			}
			if (ImGui::MenuItem("Delete"))
			{
				std::error_code ec;
				std::filesystem::remove_all(path, ec);
			}
			ImGui::EndPopup();
		}

		ImGui::PopStyleColor();

		if (m_RenamingPath == path)
		{
			ImGui::SetKeyboardFocusHere();
			ImGui::SetNextItemWidth(m_ThumbnailSize);
			if (ImGui::InputText("##RenameField", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
			{
				if (strlen(m_RenameBuffer) > 0)
				{
					std::filesystem::path newPath = path.parent_path() / m_RenameBuffer;
					std::error_code ec;
					std::filesystem::rename(path, newPath, ec);
				}
				m_RenamingPath.clear();
			}

			if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
			{
				m_RenamingPath.clear();
			}
		}
		else
		{
			std::string displayName = TruncateFileName(filename, m_ThumbnailSize);

			ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + m_ThumbnailSize);
			ImGui::Text("%s", displayName.c_str());

			if (displayName != filename && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", filename.c_str());
			}

			ImGui::PopTextWrapPos();
		}

		ImGui::NextColumn();
		ImGui::PopID();
	}

	ImGui::Columns(1);

	if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
	{
		if (ImGui::MenuItem("New Folder"))
		{
			std::filesystem::path newFolderPath = m_CurrentDirectory / "NewFolder";
			int count = 1;
			while (std::filesystem::exists(newFolderPath))
			{
				newFolderPath = m_CurrentDirectory / ("NewFolder (" + std::to_string(count++) + ")");
			}
			std::filesystem::create_directory(newFolderPath);
			StartRenaming(newFolderPath);
		}

		ImGui::Separator();

		if (ImGui::BeginMenu("Create Asset"))
		{
			auto CreateAndRenameAsset = [&](const std::string& defaultName, const std::string& content)
				{
					std::filesystem::path filePath = m_CurrentDirectory / defaultName;
					std::ofstream file(filePath);
					file << content;
					file.close();
					StartRenaming(filePath);
				};

			if (ImGui::MenuItem("Material (.hymaterial)"))
				CreateAndRenameAsset("NewMaterial.hymaterial", "{\n  \"Tint\": [1.0, 1.0, 1.0],\n  \"Roughness\": 0.5,\n  \"Metallic\": 0.0\n}");

			if (ImGui::MenuItem("Scene (.hyscene)"))
				CreateAndRenameAsset("NewScene.hyscene", "{}");

			if (ImGui::MenuItem("GLSL Shader (.glsl)"))
				CreateAndRenameAsset("NewShader.glsl", "// Hydrogen Shader\n#version 450\n\nvoid main()\n{\n}\n");

			if (ImGui::MenuItem("Lua Script (.lua)"))
				CreateAndRenameAsset("NewScript.lua", "-- Hydrogen Script\nfunction OnUpdate(dt)\nend\n");

			if (ImGui::MenuItem("Anim Graph (.hygraph)"))
				CreateAndRenameAsset("NewGraph.hygraph", "{}");

			ImGui::EndMenu();
		}

		ImGui::EndPopup();
	}

	ImGui::EndChild();
}
