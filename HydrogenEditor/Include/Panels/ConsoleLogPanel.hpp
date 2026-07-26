#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>

#include "GUISystem.hpp"

class ConsoleLogPanel : public EditorPanel
{
public:
	virtual std::string GetTitle() const override { static std::string t = "Console Output"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Bottom; }

	virtual void OnImGuiRender() override;
};
