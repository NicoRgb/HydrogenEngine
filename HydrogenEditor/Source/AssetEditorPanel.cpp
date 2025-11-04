#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>
#include <fstream>

#include "AssetEditorPanel.hpp"

void AssetEditorPanel::OpenFile(const std::filesystem::path& path)
{
    m_CurrentFile = path;

    std::ifstream in(path);
    if (!in.is_open()) return;

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    m_Editor.SetText(content);
    m_Editor.SetLanguageDefinition(GuessLanguage(path));
    m_Editor.SetShowWhitespaces(false);
    m_LastSaveStatus.clear();
}

void AssetEditorPanel::OnImGuiRender()
{
    if (m_CurrentFile.empty())
        return;

    std::string title = "Editing: " + m_CurrentFile.filename().string();
    ImGui::Begin(title.c_str());

    if (ImGui::Button("Save"))
        SaveFile();

    ImGui::SameLine();
    if (!m_LastSaveStatus.empty())
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", m_LastSaveStatus.c_str());

    ImGui::Separator();
    m_Editor.Render("TextEditor");
    ImGui::End();
}

void AssetEditorPanel::SaveFile()
{
    std::ofstream out(m_CurrentFile);
    if (!out.is_open())
    {
        m_LastSaveStatus = "Failed to save!";
        return;
    }
    out << m_Editor.GetText();
    out.close();

    m_LastSaveStatus = "Saved!";
    auto app = Hydrogen::Application::Get();
    app->MainAssetManager.LoadAssets("assets", app->_RenderContext);
    app->ReloadShader();
}

TextEditor::LanguageDefinition AssetEditorPanel::GuessLanguage(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    if (ext == ".cpp" || ext == ".h") return TextEditor::LanguageDefinition::CPlusPlus();
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag") return TextEditor::LanguageDefinition::GLSL();
    if (ext == ".lua") return TextEditor::LanguageDefinition::Lua();
    if (ext == ".json") return TextEditor::LanguageDefinition::C();
    return TextEditor::LanguageDefinition::Lua();
}
