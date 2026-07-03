#include <Hydrogen/HydrogenMain.hpp>
#include <imgui.h>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace Hydrogen;

class RuntimeApp : public Application
{
private:
	template<typename TCamera>
	void UpdateCameraViewportSize(TCamera& camera, const glm::ivec2& size)
	{
		if (camera.ViewportWidth != size.x ||
			camera.ViewportHeight != size.y)
		{
			camera.ViewportWidth = size.x;
			camera.ViewportHeight = size.y;
			camera.CalculateProj();
		}
	}

	bool UpdateCamera(float deltaTime, Entity& outEntity)
	{
		auto scene = CurrentScene->GetScene();
		Entity activeCameraEntity;

		scene->IterateComponents<CameraComponent>(
			[&](Entity entity, CameraComponent& camera)
			{
				if (camera.Active)
					activeCameraEntity = entity;
			});

		glm::ivec2 size = {
				(int)MainViewport->GetWidth(),
				(int)MainViewport->GetHeight()
		};

		if (activeCameraEntity.IsValid())
		{
			auto& camera = activeCameraEntity.GetComponent<CameraComponent>();
			camera.CalculateView(activeCameraEntity);
			UpdateCameraViewportSize(camera, size);
			outEntity = activeCameraEntity;
			return true;
		}

		return false;
	}

	float m_FPSTimer = 0.0f;
	float m_CurrentAvgFPS = 0.0f;
	float m_CurrentMinFPS = 0.0f;
	std::vector<float> m_FrameTimes;
	const size_t m_MaxSamples = 100;

	std::vector<RenderDeviceDescriptor> m_AvailableDevices;
	size_t m_SelectedDeviceIndex = 0;
	bool m_RenderDeviceChanged = false;
	RenderDeviceDescriptor m_NewRenderDevice = {};

	SwapChainSpec m_CurrentSwapChainSpec = {};
	bool m_SwapChainChanged = false;

public:
	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Runtime";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Runtime";
		ApplicationSpec.ViewportSize = { 1920, 1080 };
		ApplicationSpec.UseDebugGUI = true;
	}

	virtual void OnStartup() override
	{
		CurrentScene->GetScene()->CreateScripts();

		if (RenderInstance::Get())
		{
			m_AvailableDevices = RenderInstance::Get()->GetRenderDevices();
			m_SelectedDeviceIndex = GetCurrentRenderDeviceDesc().ID;
			m_CurrentSwapChainSpec = GetCurrentSwapChainSepc();
		}
	}

	virtual void OnShutdown() override
	{
	}

	virtual void OnUpdate(float dt) override
	{
		if (m_RenderDeviceChanged)
		{
			ChangeRenderDevice(m_NewRenderDevice);
			m_RenderDeviceChanged = false;
		}

		if (m_SwapChainChanged)
		{
			RecreateSwapchain(m_CurrentSwapChainSpec);
			m_SwapChainChanged = false;
		}

		PhysicsUpdate(dt);

		m_FrameTimes.push_back(dt);
		if (m_FrameTimes.size() > m_MaxSamples)
		{
			m_FrameTimes.erase(m_FrameTimes.begin());
		}

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

		Entity cameraEntity;
		if (UpdateCamera(dt, cameraEntity))
		{
			//MainRenderer->Render(CurrentScene->GetScene(), cameraEntity.GetComponent<CameraComponent>(), cameraEntity.GetComponent<TransformComponent>().GetPosition());
		}
	}

	virtual void OnImGuiRender() override
	{
		ImGui::Begin("Performance Statistics");

		ImGui::Text("Average FPS: %.1f", m_CurrentAvgFPS);
		ImGui::Text("Minimum FPS: %.1f", m_CurrentMinFPS);

		if (m_CurrentAvgFPS > 0.0f)
		{
			ImGui::Separator();
			ImGui::Text("Frame Time: %.2f ms", (1000.0f / m_CurrentAvgFPS));
		}

		ImGui::End();

		ImGui::Begin("Graphics Settings");

		ImGui::TextDisabled("HARDWARE CONFIGURATION");
		if (m_AvailableDevices.empty())
		{
			ImGui::Text("No valid rendering devices found!");
		}
		else
		{
			const auto& currentDevice = m_AvailableDevices[m_SelectedDeviceIndex];
			std::string previewName = currentDevice.Name;

			ImGui::Text("Target Graphics Hardware:");

			if (ImGui::BeginCombo("##DeviceSelector", previewName.c_str()))
			{
				for (size_t i = 0; i < m_AvailableDevices.size(); ++i)
				{
					const auto& device = m_AvailableDevices[i];

					double vramGB = static_cast<double>(device.VramBytes) / (1024.0 * 1024.0 * 1024.0);

					std::string typeStr = (device.Type == RenderDeviceType::DiscreteGPU) ? "Discrete" : "Integrated";
					std::string label = device.Name + " (" + typeStr + " - " + std::to_string(vramGB).substr(0, 4) + " GB)";

					bool isSelected = (m_SelectedDeviceIndex == i);
					if (ImGui::Selectable(label.c_str(), isSelected))
					{
						m_SelectedDeviceIndex = i;
						HY_APP_INFO("Selected Device {}", device.Name);
						m_RenderDeviceChanged = true;
						m_NewRenderDevice = device;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
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
					m_SwapChainChanged = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
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
					m_SwapChainChanged = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::End();
	}

	virtual void OnImGuiMenuBarRender() override
	{
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<RuntimeApp>();
}
