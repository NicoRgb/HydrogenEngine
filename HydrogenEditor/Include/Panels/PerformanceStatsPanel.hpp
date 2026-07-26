#pragma once

#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>

#include "GUISystem.hpp"

class PerformanceStatsPanel : public EditorPanel
{
public:
	virtual std::string GetTitle() const override { static std::string t = "Performance Statistics"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Right_Bottom; }

	virtual void OnUpdate(float dt) override;
	virtual void OnImGuiRender() override;

private:
	float m_FPSTimer = 0.0f;
	float m_CurrentAvgFPS = 0.0f;
	float m_CurrentMinFPS = 0.0f;

	std::vector<float> m_FrameTimes;
	const size_t m_MaxSamples = 100;
};
