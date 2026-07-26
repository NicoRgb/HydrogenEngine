#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>

#include "GUISystem.hpp"

class GraphicsSettingsPanel : public EditorPanel
{
public:
	virtual std::string GetTitle() const override { static std::string t = "Graphics Settings"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Right_Bottom; }

	virtual void OnAttach() override;
	virtual void OnImGuiRender() override;

	const Hydrogen::RenderSettings& GetSettings() const { return m_RenderSettings; }

private:
	Hydrogen::RenderSettings m_RenderSettings = {};
	Hydrogen::SwapChainSpec m_CurrentSwapChainSpec = {};

	std::vector<Hydrogen::RenderDeviceDescriptor> m_AvailableDevices;
	size_t m_SelectedDeviceIndex = 0;
};
