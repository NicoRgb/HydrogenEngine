#include "Panels/PerformanceStatsPanel.hpp"
#include "LaunchTool.hpp"

using namespace Hydrogen;

void PerformanceStatsPanel::OnUpdate(float dt)
{
	m_FrameTimes.push_back(dt);
	if (m_FrameTimes.size() > m_MaxSamples) m_FrameTimes.erase(m_FrameTimes.begin());

	m_FPSTimer += dt;
	if (m_FPSTimer >= 0.5f && !m_FrameTimes.empty())
	{
		float sum = std::accumulate(m_FrameTimes.begin(), m_FrameTimes.end(), 0.0f);
		float avgDelta = sum / m_FrameTimes.size();
		m_CurrentAvgFPS = avgDelta > 0.0f ? 1.0f / avgDelta : 0.0f;

		float maxDelta = *std::max_element(m_FrameTimes.begin(), m_FrameTimes.end());
		m_CurrentMinFPS = maxDelta > 0.0f ? 1.0f / maxDelta : 0.0f;
		m_FPSTimer = 0.0f;
	}
}

void PerformanceStatsPanel::OnImGuiRender()
{
	ImGui::Text("Average FPS: %.1f", m_CurrentAvgFPS);
	ImGui::Text("Minimum FPS: %.1f", m_CurrentMinFPS);

	if (m_CurrentAvgFPS > 0.0f)
	{
		ImGui::Separator();
		ImGui::Text("Frame Time: %.2f ms", (1000.0f / m_CurrentAvgFPS));
	}

	VmaTotalStatistics stats{};
	vmaCalculateStatistics(Application::Get()->ActiveRenderDevice->GetAllocator(), &stats);
	ImGui::Separator();
	ImGui::Text("Memory Usage:");
	ImGui::Text("  Allocations: %zu", stats.total.statistics.allocationCount);
	ImGui::Text("  Allocated: %.2f MB", static_cast<double>(stats.total.statistics.allocationBytes) / (1024.0 * 1024.0));

	ImGui::Separator();
	ImGui::Text("For advanced profiling use Tracy");
	if (ImGui::Button("Launch Tracy"))
	{
		LaunchTool("tracy-profiler.exe", "-a 127.0.0.1", std::filesystem::current_path().string());
	}
}
