#pragma once

#include <ImTextEdit.h>
#include <filesystem>
#include <string>

#include "Panel.hpp"

class AssetEditorPanel : public Panel
{
public:
    void OpenFile(const std::filesystem::path& path);
    void OnImGuiRender() override;
    const char* GetName() const override { return "Asset Editor"; }

private:
    void SaveFile();
    TextEditor::LanguageDefinition GuessLanguage(const std::filesystem::path& path);

private:
    TextEditor m_Editor;
    std::filesystem::path m_CurrentFile;
    std::string m_LastSaveStatus;
};
