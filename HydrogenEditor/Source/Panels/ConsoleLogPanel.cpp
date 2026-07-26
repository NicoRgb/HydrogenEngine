#include "Panels/ConsoleLogPanel.hpp"

using namespace Hydrogen;

void ConsoleLogPanel::OnImGuiRender()
{
	ImGui::BeginChild("LogScrollRegion");

	auto drawSink = [&](auto logger) {
		for (auto& m : logger->GetLogSink()->GetMessages())
		{
			ImVec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
			if (m.level == spdlog::level::debug) color = { 0.6f, 0.6f, 1.0f, 1.0f };
			else if (m.level == spdlog::level::warn) color = { 1.0f, 1.0f, 0.1f, 1.0f };
			else if (m.level == spdlog::level::err) color = { 1.0f, 0.3f, 0.3f, 1.0f };
			else if (m.level == spdlog::level::critical) color = { 1.0f, 0.0f, 0.0f, 1.0f };

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(m.message.c_str());
			ImGui::PopStyleColor();
		}
		};

	drawSink(EngineLogger::GetLogger());
	drawSink(AppLogger::GetLogger());

	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		ImGui::SetScrollHereY(1.0f);

	ImGui::EndChild();
}
