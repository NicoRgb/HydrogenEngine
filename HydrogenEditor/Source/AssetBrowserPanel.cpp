#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>
#include <fstream>

#include "AssetBrowserPanel.hpp"

using json = nlohmann::json;

AssetBrowserPanel::AssetBrowserPanel(const std::filesystem::path& assetDir, AssetEditorPanel& editor)
    : m_EditorPanel(editor),
    m_AssetDirectory(assetDir),
    m_CurrentDirectory(assetDir),
    m_CurrentFile("")
{
}

void AssetBrowserPanel::LoadTextures(std::shared_ptr<Hydrogen::Texture> folderTex, std::shared_ptr<Hydrogen::Texture> fileTex)
{
    m_FolderTexture = folderTex;
    m_FileTexture = fileTex;
}

void AssetBrowserPanel::DrawFileConfig(json& j) {
    std::string assetType = j["type"].get<std::string>();
    ImGui::Text("%s", j["name"].get<std::string>().c_str());
    ImGui::Indent();
    ImGui::Text("Type: %s", assetType.c_str());
    ImGui::Unindent();

    if (ImGui::TreeNode("Preferences"))
    {
        if (assetType == "Shader")
        {
            static const char* shaderStages[] =
            {
                "vertex",
                "fragment",
            };

            std::string stage = j["preferences"].value("stage", "vertex");
            int currentIndex = 0;
            for (int i = 0; i < IM_ARRAYSIZE(shaderStages); ++i)
            {
                if (stage == shaderStages[i])
                {
                    currentIndex = i;
                    break;
                }
            }

            if (ImGui::BeginCombo("Shader Stage", shaderStages[currentIndex]))
            {
                for (int i = 0; i < IM_ARRAYSIZE(shaderStages); ++i)
                {
                    bool isSelected = (currentIndex == i);
                    if (ImGui::Selectable(shaderStages[i], isSelected))
                    {
                        if (stage != shaderStages[i])
                        {
                            stage = shaderStages[i];
                            j["preferences"]["stage"] = stage;

                            if (m_CurrentFile.has_filename())
                            {
                                std::filesystem::path assetFile = m_CurrentFile.string() + ".hyasset";
                                std::ofstream fout(assetFile);
                                if (fout.is_open())
                                {
                                    fout << j.dump(4);
                                    fout.close();
                                    Hydrogen::Application::Get()->MainAssetManager.LoadAssets(
                                        "assets", Hydrogen::Application::Get()->_RenderContext
                                    );
                                    Hydrogen::Application::Get()->ReloadShader();
                                }
                            }
                        }
                        currentIndex = i;
                    }
                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        ImGui::TreePop();
    }
}

void AssetBrowserPanel::OnImGuiRender()
{
    ImGui::Begin(GetName());

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
            ImGui::ImageButton("[DIR]", m_FolderTexture->GetImGuiImage(), { thumbnailSize, thumbnailSize });
        }
        else
        {
            ImGui::ImageButton("[FILE]", m_FileTexture->GetImGuiImage(), { thumbnailSize, thumbnailSize });

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
            else
            {
                std::string ext = path.extension().string();
                if (ext == ".json" || ext == ".shader" || ext == ".glsl" || ext == ".lua" || ext == ".txt")
                    m_EditorPanel.OpenFile(path);
            }
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !entry.is_directory())
        {
            m_CurrentFile = path;
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

    ImGui::End();

    ImGui::Begin("Asset Inspector");
    if (m_CurrentFile.empty())
    {
        ImGui::Text("No asset selected");
    }
    else
    {
        std::ifstream fin(m_CurrentFile.string() + ".hyasset");
        if (!fin)
        {
            Hydrogen::Application::Get()->MainAssetManager.LoadAssets("assets", Hydrogen::Application::Get()->_RenderContext);
            fin = std::ifstream(m_CurrentFile.string() + ".hyasset");
        }

        if (fin.is_open())
        {
            auto config = json::parse(fin);
            fin.close();
            DrawFileConfig(config);
        }
        else
        {
            ImGui::Text("Failed to open asset metadata file");
        }
    }
    ImGui::End();
}
