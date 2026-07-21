#pragma once

#include <Hydrogen/HydrogenMain.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <numeric>

#include "GUISystem.hpp"

using namespace Hydrogen;

extern ImGuiTextureCache TextureCache;
extern VkSampler ImguiSampler;

// ============================================================
// Console Output Panel
// ============================================================
class ConsoleLogPanel : public EditorPanel
{
public:
	virtual std::string GetTitle() const override { static std::string t = "Console Output"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Bottom; }

	virtual void OnImGuiRender() override
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
};

// ============================================================
// Performance Stats Panel
// ============================================================
class PerformanceStatsPanel : public EditorPanel
{
private:
	float* m_AvgFPS;
	float* m_MinFPS;

public:
	PerformanceStatsPanel(float* avgFPS, float* minFPS)
		: m_AvgFPS(avgFPS), m_MinFPS(minFPS) {
	}

	virtual std::string GetTitle() const override { static std::string t = "Performance Statistics"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Right_Bottom; }

	virtual void OnImGuiRender() override
	{
		ImGui::Text("Average FPS: %.1f", *m_AvgFPS);
		ImGui::Text("Minimum FPS: %.1f", *m_MinFPS);

		if (*m_AvgFPS > 0.0f)
		{
			ImGui::Separator();
			ImGui::Text("Frame Time: %.2f ms", (1000.0f / *m_AvgFPS));
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
};

// ============================================================
// Graphics Settings Panel
// ============================================================
class GraphicsSettingsPanel : public EditorPanel
{
private:
	std::vector<RenderDeviceDescriptor>* m_AvailableDevices;
	size_t* m_SelectedDeviceIndex;
	bool* m_RenderDeviceChanged;
	RenderDeviceDescriptor* m_NewRenderDevice;
	SwapChainSpec* m_CurrentSwapChainSpec;
	bool* m_SwapChainChanged;
	RenderSettings* m_RenderSettings;

public:
	GraphicsSettingsPanel(
		std::vector<RenderDeviceDescriptor>* devices,
		size_t* selectedIndex,
		bool* devChanged,
		RenderDeviceDescriptor* newDev,
		SwapChainSpec* swapSpec,
		bool* swapChanged,
		RenderSettings* settings
	) : m_AvailableDevices(devices), m_SelectedDeviceIndex(selectedIndex),
		m_RenderDeviceChanged(devChanged), m_NewRenderDevice(newDev),
		m_CurrentSwapChainSpec(swapSpec), m_SwapChainChanged(swapChanged),
		m_RenderSettings(settings) {
	}

	virtual std::string GetTitle() const override { static std::string t = "Graphics Settings"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Right_Bottom; }

	virtual void OnImGuiRender() override
	{
		ImGui::TextDisabled("HARDWARE CONFIGURATION");
		if (m_AvailableDevices->empty())
		{
			ImGui::Text("No valid rendering devices found!");
		}
		else
		{
			const auto& currentDevice = (*m_AvailableDevices)[*m_SelectedDeviceIndex];
			std::string previewName = currentDevice.Name;

			ImGui::Text("Target Graphics Hardware:");

			if (ImGui::BeginCombo("##DeviceSelector", previewName.c_str()))
			{
				for (size_t i = 0; i < m_AvailableDevices->size(); ++i)
				{
					const auto& device = (*m_AvailableDevices)[i];
					double vramGB = static_cast<double>(device.VramBytes) / (1024.0 * 1024.0 * 1024.0);
					std::string typeStr = (device.Type == RenderDeviceType::DiscreteGPU) ? "Discrete" : "Integrated";
					std::string label = device.Name + " (" + typeStr + " - " + std::to_string(vramGB).substr(0, 4) + " GB)";

					bool isSelected = (*m_SelectedDeviceIndex == i);
					if (ImGui::Selectable(label.c_str(), isSelected))
					{
						*m_SelectedDeviceIndex = i;
						HY_APP_INFO("Selected Device {}", device.Name);
						*m_RenderDeviceChanged = true;
						*m_NewRenderDevice = device;
					}

					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::Text("Device Specifications:");
			ImGui::Text("  Internal ID: %zu", currentDevice.ID);
			ImGui::Text("  Total VRAM:  %.2f GB", static_cast<double>(currentDevice.VramBytes) / (1024.0 * 1024.0 * 1024.0));
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("SWAPCHAIN & DISPLAY PREFERENCES");

		const char* presentModeLabels[] = { "Immediate (Uncapped / Tearing)", "VSync (Capped)", "Mailbox (Triple Buffering)" };
		int currentPresentInt = static_cast<int>(m_CurrentSwapChainSpec->VsyncPreference);

		ImGui::Text("Vertical Sync Mode:");
		if (ImGui::BeginCombo("##VsyncSelector", presentModeLabels[currentPresentInt]))
		{
			for (int i = 0; i < 3; ++i)
			{
				bool isSelected = (currentPresentInt == i);
				if (ImGui::Selectable(presentModeLabels[i], isSelected))
				{
					m_CurrentSwapChainSpec->VsyncPreference = static_cast<PresentMode>(i);
					HY_APP_INFO("Present Mode changed to: {}", presentModeLabels[i]);
					*m_SwapChainChanged = true;
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();

		const char* colorFormatLabels[] = { "RGBA8 SRGB", "BGRA8 SRGB", "HDR10 (High Dynamic Range)" };
		int currentColorInt = static_cast<int>(m_CurrentSwapChainSpec->ColorPreference);

		ImGui::Text("Color Format Profile:");
		if (ImGui::BeginCombo("##ColorSelector", colorFormatLabels[currentColorInt]))
		{
			for (int i = 0; i < 3; ++i)
			{
				bool isSelected = (currentColorInt == i);
				if (ImGui::Selectable(colorFormatLabels[i], isSelected))
				{
					m_CurrentSwapChainSpec->ColorPreference = static_cast<ColorFormat>(i);
					HY_APP_INFO("Color Format profile changed to: {}", colorFormatLabels[i]);
					*m_SwapChainChanged = true;
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("RENDERING SETTINGS");
		ImGui::Checkbox("Wireframe Mode", &m_RenderSettings->Debug.WireframeMode);
		ImGui::Checkbox("Tone Mapping", &m_RenderSettings->PostProcessing.ToneMapping);
		ImGui::Checkbox("Bloom", &m_RenderSettings->PostProcessing.BloomEnabled);

		int bloomIterations = (int)m_RenderSettings->PostProcessing.BloomIterations;
		if (ImGui::InputInt("Bloom Iterations", &bloomIterations))
			m_RenderSettings->PostProcessing.BloomIterations = (uint8_t)bloomIterations;
	}
};

// ============================================================
// Scene Viewport Panel
// ============================================================
class SceneViewportPanel : public EditorPanel
{
private:
	VkImageView m_CurrentTextureView = VK_NULL_HANDLE;
	glm::vec2 m_CurrentSize{ 1920.0f, 1080.0f };

public:
	virtual void OnAttach()
	{
		Dockspace->GetEventBus().Subscribe<SceneViewportRenderedEvent>([this](const SceneViewportRenderedEvent& e) {
			m_CurrentTextureView = e.TextureView;
			});
	}

	virtual std::string GetTitle() const override { static std::string t = "Scene"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Center; }

	virtual void OnImGuiRender() override
	{
		bool isVisible = ImGui::Begin(GetID().c_str(), &m_IsOpen);

		ImVec2 viewportMinRegion = ImGui::GetWindowContentRegionMin();
		ImVec2 viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		ImVec2 viewportOffset = ImGui::GetWindowPos();

		ImVec2 contentRegion = ImGui::GetContentRegionAvail();

		SceneViewportStateEvent stateEvent;
		stateEvent.Size = { contentRegion.x, contentRegion.y };
		stateEvent.BoundsMin = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		stateEvent.BoundsMax = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };
		stateEvent.IsVisible = isVisible;
		stateEvent.IsHovered = ImGui::IsWindowHovered();

		Dockspace->GetEventBus().Publish(stateEvent);

		if (isVisible)
		{
			if (m_CurrentTextureView != VK_NULL_HANDLE && ImguiSampler != VK_NULL_HANDLE)
			{
				ImTextureID textureID = TextureCache.GetTextureID(m_CurrentTextureView, ImguiSampler);
				ImGui::Image(textureID, contentRegion);
			}
		}

		ImGui::End();
	}
};

// ============================================================
// Game Viewport Panel
// ============================================================
class GameViewportPanel : public EditorPanel
{
private:
	VkImageView m_CurrentTextureView = VK_NULL_HANDLE;

public:
	void OnAttach()
	{
		Dockspace->GetEventBus().Subscribe<GameViewportRenderedEvent>([this](const GameViewportRenderedEvent& e) {
			m_CurrentTextureView = e.TextureView;
			});
	}

	virtual std::string GetTitle() const override { static std::string t = "Game"; return t; }
	virtual DockDirection GetDefaultDockDirection() const override { return DockDirection::Center; }

	virtual void OnImGuiRender() override
	{
		bool isVisible = ImGui::Begin(GetID().c_str(), &m_IsOpen);

		ImVec2 contentRegion = ImGui::GetContentRegionAvail();

		GameViewportStateEvent stateEvent;
		stateEvent.Size = { contentRegion.x, contentRegion.y };
		stateEvent.IsVisible = isVisible;

		Dockspace->GetEventBus().Publish(stateEvent);

		if (isVisible && m_CurrentTextureView != VK_NULL_HANDLE && ImguiSampler != VK_NULL_HANDLE)
		{
			ImTextureID textureID = TextureCache.GetTextureID(m_CurrentTextureView, ImguiSampler);
			ImGui::Image(textureID, contentRegion);
		}

		ImGui::End();
	}
};
