#include "Editors/CodeEditor.hpp"
#include <Hydrogen/Application.hpp>

void CodeEditor::OnAttach()
{
	std::ifstream in(m_FilePath);
	if (!in.is_open()) return;

	std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	in.close();

	m_Editor.SetText(content);
	m_Editor.SetLanguageDefinition(GuessLanguage());
	m_Editor.SetShowWhitespaces(false);
}

void CodeEditor::OnImGuiRender()
{
	m_Editor.Render("TextEditor");
	if (m_Editor.IsTextChanged())
	{
		m_IsDirty = true;
	}
}

void CodeEditor::OnSave()
{
	std::ofstream out(m_FileName);
	if (!out.is_open())
	{
		return;
	}
	out << m_Editor.GetText();
	out.close();

	auto app = Hydrogen::Application::Get();
	app->MainAssetManager.ReloadAsset(m_FileName);

	m_IsDirty = false;
}

TextEditor::LanguageDefinition CodeEditor::GuessLanguage()
{
	auto ext = std::filesystem::path(m_FilePath).extension().string();
	if (ext == ".glsl" || ext == ".vert" || ext == ".frag") return TextEditor::LanguageDefinition::GLSL();
	if (ext == ".lua") return TextEditor::LanguageDefinition::Lua();
	return TextEditor::LanguageDefinition::Lua();
}
