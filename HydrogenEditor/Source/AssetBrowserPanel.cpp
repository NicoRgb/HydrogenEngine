#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>
#include <fstream>

#include "AssetBrowserPanel.hpp"

using json = nlohmann::json;
using namespace Hydrogen;

extern ImGuiTextureCache TextureCache;
extern VkSampler ImguiSampler;

void AssetBrowserPanel::OnAttach()
{
	m_AssetDirectory = Application::Get()->MainAssetManager.GetAssetDirectory();
	m_CurrentDirectory = m_AssetDirectory;
}

void AssetBrowserPanel::OnDetach()
{
}

void AssetBrowserPanel::OnImGuiRender()
{
	const auto& folderTexture = Application::Get()->MainAssetManager.GetAsset<TextureAsset>("folder_icon.png")->GetTexture(Application::Get()->GetRenderDevice());
	const auto& fileTexture = Application::Get()->MainAssetManager.GetAsset<TextureAsset>("file_icon.png")->GetTexture(Application::Get()->GetRenderDevice());

	if (m_CurrentDirectory != m_AssetDirectory)
	{
		if (ImGui::Button("<-"))
		{
			m_CurrentDirectory = m_CurrentDirectory.parent_path();
		}
	}

	static float thumbnailSize = 64.0f;
	static float padding = 16.0f;
	float cellSize = thumbnailSize + padding;
	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = (int)(panelWidth / cellSize);
	if (columnCount < 1)
		columnCount = 1;

	ImGui::Columns(columnCount, 0, false);

	for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
	{
		const auto& path = entry.path();
		if (path.extension() == ".hyasset")
			continue;

		std::string filename = path.filename().string();

		ImGui::PushID(filename.c_str());
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));

		if (entry.is_directory())
		{
			ImGui::ImageButton("[DIR]", TextureCache.GetTextureID(folderTexture->GetImageView(), ImguiSampler), { thumbnailSize, thumbnailSize });
		}
		else
		{
			ImGui::ImageButton("[FILE]", TextureCache.GetTextureID(fileTexture->GetImageView(), ImguiSampler), { thumbnailSize, thumbnailSize });

			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
			{
				std::string pathStr = path.string();
				ImGui::SetDragDropPayload("ASSET_FILE", pathStr.c_str(), pathStr.size() + 1);
				ImGui::Text("%s", filename.c_str());
				ImGui::EndDragDropSource();
			}
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (entry.is_directory())
			{
				m_CurrentDirectory /= path.filename();
			}
		}

		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !entry.is_directory())
		{
			Dockspace->GetEventBus().Publish(AssetSelectedEvent{
				(m_CurrentDirectory / path.filename()).string()
				});
		}

		ImGui::PopStyleColor();
		ImGui::TextWrapped("%s", filename.c_str());
		ImGui::NextColumn();
		ImGui::PopID();
	}

	ImGui::Columns(1);

	if (ImGui::BeginPopupContextWindow())
	{
		if (ImGui::MenuItem("Create Folder"))
		{
			showCreateFolderDialog = true;
			inputName[0] = '\0';
		}

		if (ImGui::MenuItem("Create File"))
		{
			showCreateFileDialog = true;
			inputName[0] = '\0';
		}
		ImGui::EndPopup();
	}

	// Folder creation dialog
	if (showCreateFolderDialog)
	{
		ImGui::OpenPopup("Create Folder");
		showCreateFolderDialog = false;
	}
	if (ImGui::BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("Folder Name", inputName, sizeof(inputName));
		if (ImGui::Button("Create"))
		{
			if (strlen(inputName) > 0) {
				std::filesystem::path folderPath = m_CurrentDirectory / inputName;
				std::error_code ec;
				std::filesystem::create_directory(folderPath, ec);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	if (showCreateFileDialog)
	{
		ImGui::OpenPopup("Create File");
		showCreateFileDialog = false;
	}
	if (ImGui::BeginPopupModal("Create File", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputText("File Name", inputName, sizeof(inputName));
		if (ImGui::Button("Create"))
		{
			if (strlen(inputName) > 0) {
				std::filesystem::path filePath = m_CurrentDirectory / inputName;
				std::ofstream file(filePath);
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
}
