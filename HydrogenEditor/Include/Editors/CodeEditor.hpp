#pragma once

#include "GUISystem.hpp"
#include <ImTextEdit.h>

class CodeEditor : public DocumentTab
{
public:
	CodeEditor(const std::string& filePath)
		: DocumentTab(filePath) {}

	virtual void OnAttach();
	virtual void OnImGuiRender();
	virtual void OnSave();

private:
	TextEditor m_Editor;
	TextEditor::LanguageDefinition GuessLanguage();
};
