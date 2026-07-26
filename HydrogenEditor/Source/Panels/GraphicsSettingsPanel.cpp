#include "Panels/GraphicsSettingsPanel.hpp"

using namespace Hydrogen;

void GraphicsSettingsPanel::OnAttach()
{
	m_AvailableDevices = RenderInstance::Get()->GetRenderDevices();
	m_SelectedDeviceIndex = Application::Get()->GetCurrentRenderDeviceDesc().ID;
	m_CurrentSwapChainSpec = Application::Get()->GetCurrentSwapChainSepc();

	m_RenderSettings.Rendering.Skybox = Application::Get()->MainAssetManager.GetAsset<CubeMapAsset>("sky.hycube");
}

void GraphicsSettingsPanel::OnImGuiRender()
{
	HardwareChangeEvent Event;

	ImGui::TextDisabled("RENDERING");

	AssetPicker("Skybox", m_RenderSettings.Rendering.Skybox);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextDisabled("HARDWARE CONFIGURATION");
	if (m_AvailableDevices.empty())
	{
		ImGui::Text("No valid rendering devices found!");
	}
	else
	{
		const auto& currentDevice = (m_AvailableDevices)[m_SelectedDeviceIndex];
		std::string previewName = currentDevice.Name;

		ImGui::Text("Target Graphics Hardware:");

		if (ImGui::BeginCombo("##DeviceSelector", previewName.c_str()))
		{
			for (size_t i = 0; i < m_AvailableDevices.size(); ++i)
			{
				const auto& device = (m_AvailableDevices)[i];
				double vramGB = static_cast<double>(device.VramBytes) / (1024.0 * 1024.0 * 1024.0);
				std::string typeStr = (device.Type == RenderDeviceType::DiscreteGPU) ? "Discrete" : "Integrated";
				std::string label = device.Name + " (" + typeStr + " - " + std::to_string(vramGB).substr(0, 4) + " GB)";

				bool isSelected = (m_SelectedDeviceIndex == i);
				if (ImGui::Selectable(label.c_str(), isSelected))
				{
					m_SelectedDeviceIndex = i;
					HY_APP_INFO("Selected Device {}", device.Name);
					Event.RenderDeviceChanged = true;
					Event.RenderDeviceDesc = device;
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
	int currentPresentInt = static_cast<int>(m_CurrentSwapChainSpec.VsyncPreference);

	ImGui::Text("Vertical Sync Mode:");
	if (ImGui::BeginCombo("##VsyncSelector", presentModeLabels[currentPresentInt]))
	{
		for (int i = 0; i < 3; ++i)
		{
			bool isSelected = (currentPresentInt == i);
			if (ImGui::Selectable(presentModeLabels[i], isSelected))
			{
				m_CurrentSwapChainSpec.VsyncPreference = static_cast<PresentMode>(i);
				HY_APP_INFO("Present Mode changed to: {}", presentModeLabels[i]);
				Event.SwapChainChanged = true;
			}
			if (isSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Spacing();

	const char* colorFormatLabels[] = { "RGBA8 SRGB", "BGRA8 SRGB", "HDR10 (High Dynamic Range)" };
	int currentColorInt = static_cast<int>(m_CurrentSwapChainSpec.ColorPreference);

	ImGui::Text("Color Format Profile:");
	if (ImGui::BeginCombo("##ColorSelector", colorFormatLabels[currentColorInt]))
	{
		for (int i = 0; i < 3; ++i)
		{
			bool isSelected = (currentColorInt == i);
			if (ImGui::Selectable(colorFormatLabels[i], isSelected))
			{
				m_CurrentSwapChainSpec.ColorPreference = static_cast<ColorFormat>(i);
				HY_APP_INFO("Color Format profile changed to: {}", colorFormatLabels[i]);
				Event.SwapChainChanged = true;
			}
			if (isSelected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	ImGui::TextDisabled("RENDERING SETTINGS");
	ImGui::Checkbox("Wireframe Mode", &m_RenderSettings.Debug.WireframeMode);
	ImGui::Checkbox("Tone Mapping", &m_RenderSettings.PostProcessing.ToneMapping);
	ImGui::Checkbox("Bloom", &m_RenderSettings.PostProcessing.BloomEnabled);

	int bloomIterations = (int)m_RenderSettings.PostProcessing.BloomIterations;
	if (ImGui::InputInt("Bloom Iterations", &bloomIterations))
		m_RenderSettings.PostProcessing.BloomIterations = (uint8_t)bloomIterations;

	if (Event.RenderDeviceChanged || Event.SwapChainChanged)
	{
		Event.SwapChainSpec = m_CurrentSwapChainSpec;
		Dockspace->GetEventBus().Publish<HardwareChangeEvent>(Event);
	}
}
