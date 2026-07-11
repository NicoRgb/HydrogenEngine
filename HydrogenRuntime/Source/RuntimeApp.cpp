#include <Hydrogen/HydrogenMain.hpp>
#include <imgui.h>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace Hydrogen;

class RuntimeApp : public Application
{
private:
	ImGuiTextureCache m_ImGuiTextureCache;
	ImVec2 m_OldViewportContentRegion = ImVec2(1920, 1080);
	ImVec2 m_ViewportContentRegion = ImVec2(1920, 1080);

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
				(int)m_ViewportContentRegion.x,
				(int)m_ViewportContentRegion.y
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

	bool m_WireframeMode = false;

	std::vector<RenderDeviceDescriptor> m_AvailableDevices;
	size_t m_SelectedDeviceIndex = 0;
	bool m_RenderDeviceChanged = false;
	RenderDeviceDescriptor m_NewRenderDevice = {};

	SwapChainSpec m_CurrentSwapChainSpec = {};
	bool m_SwapChainChanged = false;

	std::unique_ptr<Renderer> m_Renderer;
	std::unique_ptr<RenderBuffer> m_UniformBuffer;

	struct UniformBuffer
	{
		glm::mat4 View;
		glm::mat4 Proj;
		glm::vec3 ViewPos;
		float Padding;
	};

	struct PushConstants
	{
		glm::mat4 Model;
		glm::vec4 Color;
		int TexIndex;
		int Padding[3];
	};

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
		m_Renderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());

		CurrentScene->GetScene()->CreateScripts();

		if (RenderInstance::Get())
		{
			m_AvailableDevices = RenderInstance::Get()->GetRenderDevices();
			m_SelectedDeviceIndex = GetCurrentRenderDeviceDesc().ID;
			m_CurrentSwapChainSpec = GetCurrentSwapChainSepc();
		}

		BufferDescription desc = { sizeof(UniformBuffer), BufferType::Uniform, true, true };
		m_UniformBuffer = std::make_unique<RenderBuffer>(ActiveRenderDevice.get(), desc);
	}

	virtual void OnShutdown() override
	{
		m_ImGuiTextureCache.Clear();
		m_UniformBuffer.reset();
		m_Renderer.reset();
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

		RenderScene(dt);
	}

	virtual void OnImGuiRender() override
	{
		m_Renderer->BeginImGuiFrame();

		static bool dockingEnabled = true;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		ImGui::Begin("MainDockSpace", &dockingEnabled, window_flags);

		ImGui::PopStyleVar(2);
		ImGuiID dockspace_id = ImGui::GetID("DockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
		OnImGuiMenuBarRender();
		ImGui::End();

		ImGui::Begin("Performance Statistics");

		ImGui::Text("Average FPS: %.1f", m_CurrentAvgFPS);
		ImGui::Text("Minimum FPS: %.1f", m_CurrentMinFPS);

		if (m_CurrentAvgFPS > 0.0f)
		{
			ImGui::Separator();
			ImGui::Text("Frame Time: %.2f ms", (1000.0f / m_CurrentAvgFPS));
		}

		VmaTotalStatistics stats{};
		vmaCalculateStatistics(ActiveRenderDevice->GetAllocator(), &stats);

		ImGui::Separator();

		ImGui::Text("Memory Usage Statistics:");
		ImGui::Text("  Total Allocations: %zu", stats.total.statistics.allocationCount);
		ImGui::Text("  Total Allocated Memory: %.2f MB", static_cast<double>(stats.total.statistics.allocationBytes) / (1024.0 * 1024.0));
		ImGui::Text("  Reserved Memory: %.2f MB", static_cast<double>(stats.total.statistics.blockBytes) / (1024.0 * 1024.0));

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

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextDisabled("RENDERING SETTINGS");
		ImGui::Checkbox("Wireframe Mode", &m_WireframeMode);
		ImGui::Spacing();

		ImGui::End();
	}

	virtual void OnImGuiMenuBarRender() override
	{
	}

	virtual void OnSwapchainRecreation() override
	{
		m_ImGuiTextureCache.Clear();
		m_Renderer->UpdateSwapChain(ActiveSwapChain.get());
	}

	virtual void OnRenderDeviceChangeStart() override
	{
		m_UniformBuffer.reset();
		m_ImGuiTextureCache.Clear();
		m_Renderer.reset();
	}

	virtual void OnRenderDeviceChangeFinish() override
	{
		BufferDescription desc = { sizeof(UniformBuffer), BufferType::Uniform, true, true };
		m_UniformBuffer = std::make_unique<RenderBuffer>(ActiveRenderDevice.get(), desc);

		m_Renderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());
	}

private:
	void RenderScene(float dt)
	{
		Entity cameraEntity;
		if (!UpdateCamera(dt, cameraEntity))
		{
			return;
		}

		const auto& camera = cameraEntity.GetComponent<CameraComponent>();
		const auto& cameraPos = cameraEntity.GetComponent<TransformComponent>().GetPosition();
		const auto& scene = CurrentScene->GetScene();

		m_Renderer->Render(
			[this, camera, cameraPos, scene](RenderGraph* graph) -> const std::vector<DescriptorBindingValue>
			{
				if (m_ViewportContentRegion.x != m_OldViewportContentRegion.x || m_ViewportContentRegion.y != m_OldViewportContentRegion.y)
				{
					m_OldViewportContentRegion = m_ViewportContentRegion;
					m_Renderer->ClearCache();
					m_ImGuiTextureCache.Clear();
				}

				RgTextureDesc finalSceneDesc = {};
				finalSceneDesc.Width = (uint32_t)m_ViewportContentRegion.x;
				finalSceneDesc.Height = (uint32_t)m_ViewportContentRegion.y;
				finalSceneDesc.Format = TextureFormat::RGBA8_SRGB;
				auto finalTexture = graph->CreateTexture(finalSceneDesc);

				auto swapChainImage = ActiveSwapChain->AcquireNextImage(graph, m_Renderer->GetImageAvailableSemaphore());

				auto vertexShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("TriangleVertexShader.glsl");
				auto fragmentShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("TriangleFragmentShader.glsl");

				VkSampler viewportSampler = m_Renderer->GetImguiSampler();

				UniformBuffer cameraInfo = {};
				cameraInfo.View = camera.View;
				cameraInfo.Proj = camera.Proj;
				cameraInfo.ViewPos = cameraPos;

				m_UniformBuffer->UploadData((void*)&cameraInfo, sizeof(UniformBuffer), 0);

				RenderDevice* renderDevice = ActiveRenderDevice.get();
				auto wireframeMode = m_WireframeMode;

				graph->AddPass("Triangle", {},
					[finalTexture](RgPassBuilder& builder)
					{
						builder.WriteColor(finalTexture);
					},
					[renderDevice, scene, vertexShader, fragmentShader, wireframeMode](RgCommandList& cmd)
					{
						PipelineSpec trianglePipeline = {};
						trianglePipeline.VertexBufferLayout = { { VertexElementType::Float3 }, { VertexElementType::Float2 }, { VertexElementType::Float3 }, { VertexElementType::Float3 } };
						trianglePipeline.ColorBlending = { BlendMode::None };
						trianglePipeline.PushConstants = { { sizeof(PushConstants), (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex) } };
						if (wireframeMode)
						{
							trianglePipeline.PolygonMode = PolygonModeStyle::Line;
						}
						cmd.BindPipeline(vertexShader, fragmentShader, trianglePipeline);

						scene->IterateComponents<MeshRendererComponent>([&cmd, renderDevice](Entity e, MeshRendererComponent mesh)
							{
								PushConstants pc = {};
								pc.Model = e.GetComponent<TransformComponent>().Transform;
								pc.Color = glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
								pc.TexIndex = 0;

								cmd.PushConstants(&pc, sizeof(PushConstants), 0, (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex));

								cmd.BindVertexBuffer(mesh.Mesh->GetVertexBuffer(renderDevice));
								cmd.BindIndexBuffer(mesh.Mesh->GetIndexBuffer(renderDevice));
								cmd.DrawIndexed(mesh.Mesh->GetIndexCount());
							});
					});

				auto& contentRegion = m_ViewportContentRegion;
				auto& textureCache = m_ImGuiTextureCache;

				graph->AddPass("ImGui", {},
					[finalTexture, swapChainImage](RgPassBuilder& builder)
					{
						builder.WriteColor(swapChainImage);
						builder.ReadTexture(finalTexture);
					},
					[viewportSampler, finalSceneDesc, finalTexture, vertexShader, fragmentShader, &contentRegion, &textureCache](RgCommandList& cmd)
					{
						ImGui::Begin("Viewport");
						contentRegion = ImGui::GetContentRegionAvail();
						ImGui::Image(textureCache.GetTextureID(cmd.GetTextureView(finalTexture).ImageView, viewportSampler), ImVec2((float)finalSceneDesc.Width, (float)finalSceneDesc.Height));
						ImGui::End();

						ImGui::Render();
						ImDrawData* drawData = ImGui::GetDrawData();

						ImGui_ImplVulkan_RenderDrawData(drawData, cmd.GetCommandBuffer());
					});

				graph->Compile({ { 0, DescriptorType::UniformBuffer, 1, ShaderStage::Vertex } });

				DescriptorBindingValue value;
				value.Type = DescriptorType::UniformBuffer;
				value.RenderBuffers.push_back(m_UniformBuffer.get());

				return { value };
			}, true);
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<RuntimeApp>();
}
