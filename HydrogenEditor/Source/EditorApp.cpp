#include <Hydrogen/HydrogenMain.hpp>

#include "AssetBrowserPanel.hpp"
#include "AssetEditorPanel.hpp"
#include "InspectorPanel.hpp"
#include "SceneHierarchyPanel.hpp"
#include "ImGuizmo.h"

using namespace Hydrogen;

static AssetEditorPanel      _EditorPanel;
static AssetBrowserPanel     _BrowserPanel("assets", _EditorPanel);
static SceneHierarchyPanel   _SceneHierarchy;
static InspectorPanel        _Inspector;

static ImGuizmo::OPERATION GuizmoTool = ImGuizmo::TRANSLATE;

ImGuiTextureCache TextureCache;

std::shared_ptr<Scene> SavedScene;

class EditorApp : public Application
{
private:
	bool m_IsSimulating = false;

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

	FreeCamera FreeCam;
	//std::shared_ptr<CubeMap> Skybox;

	ImVec2 m_OldViewportContentRegion = ImVec2(1920, 1080);
	ImVec2 m_ViewportContentRegion = ImVec2(1920, 1080);

	std::unique_ptr<Renderer> m_Renderer;

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

private:

	// ============================================================
	// Simulation Control
	// ============================================================

	void StartSimulation()
	{
		SavedScene = std::make_shared<Scene>();
		SavedScene->DeserializeScene(
			CurrentScene->GetScene()->SerializeScene(),
			&MainAssetManager
		);

		CurrentScene->GetScene()->CreateScripts();

		m_IsSimulating = true;
	}

	void StopSimulation()
	{
		CurrentScene->ClearScene();
		CurrentScene->GetScene()->DeserializeScene(
			SavedScene->SerializeScene(),
			&MainAssetManager
		);

		m_IsSimulating = false;
	}

	void ToggleSimulation()
	{
		if (m_IsSimulating)
			StopSimulation();
		else
			StartSimulation();

		_SceneHierarchy.SetContext(CurrentScene->GetScene());
	}

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

	// ============================================================
	// Log Rendering
	// ============================================================

	ImVec4 GetLogColor(spdlog::level::level_enum level)
	{
		switch (level)
		{
		case spdlog::level::debug:    return { 0.6f, 0.6f, 1, 1 };
		case spdlog::level::warn:     return { 1, 1, 0.1f, 1 };
		case spdlog::level::err:      return { 1, 0.3f, 0.3f, 1 };
		case spdlog::level::critical: return { 1, 0, 0, 1 };
		default:                     return { 1, 1, 1, 1 };
		}
	}

	void DrawSettingsWindow()
	{
	}

	void DrawLogMessages()
	{
		ImGui::Begin("Log");
		ImGui::BeginChild("LogScrollRegion");

		auto drawSink = [&](auto logger)
			{
				for (auto& m : logger->GetLogSink()->GetMessages())
				{
					ImVec4 color = GetLogColor(m.level);
					ImGui::PushStyleColor(ImGuiCol_Text, color);
					ImGui::TextUnformatted(m.message.c_str());
					ImGui::PopStyleColor();
				}
			};

		drawSink(EngineLogger::GetLogger());
		drawSink(AppLogger::GetLogger());

		ImGui::EndChild();
		ImGui::End();
	}

	// ============================================================
	// Viewport + Gizmo
	// ============================================================

	void DrawGizmo()
	{
		auto selectedEntity = _SceneHierarchy.GetSelectedEntity();
		if (!selectedEntity.IsValid())
			return;

		ImGuizmo::SetDrawlist();

		auto& tc = selectedEntity.GetComponent<TransformComponent>();
		glm::mat4 transform = tc.Transform;

		glm::mat4 view = FreeCam.View;
		glm::mat4 proj = FreeCam.Proj;
		proj[1][1] *= -1;

		ImGuizmo::Manipulate(
			glm::value_ptr(view),
			glm::value_ptr(proj),
			GuizmoTool,
			ImGuizmo::WORLD,
			glm::value_ptr(transform)
		);

		if (ImGuizmo::IsUsing())
			tc.Transform = transform;
	}

	void CalculateFPS(float dt)
	{
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
	}

public:

	// ============================================================
	// Application Lifecycle
	// ============================================================

	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Editor";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Editor";
		ApplicationSpec.ViewportSize = { 1920, 1080 };
		ApplicationSpec.UseDebugGUI = true;
	}

	virtual void OnStartup() override
	{
		auto folderTextureAsset =
			MainAssetManager.GetAsset<TextureAsset>("folder_icon.png");

		auto fileTextureAsset =
			MainAssetManager.GetAsset<TextureAsset>("file_icon.png");

		_BrowserPanel.LoadTextures(
			folderTextureAsset->GetTexture(ActiveRenderDevice.get()),
			fileTextureAsset->GetTexture(ActiveRenderDevice.get()));

		_SceneHierarchy.SetContext(CurrentScene->GetScene());

		SavedScene = std::make_shared<Scene>();

		FreeCam.ViewportWidth = MainViewport->GetWidth();
		FreeCam.ViewportHeight = MainViewport->GetHeight();
		FreeCam.SetPosition(glm::vec3(0.0f, 1.0f, 5.0f));
		FreeCam.CalculateProj();
		FreeCam.Active = true;

		m_Renderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());

		CurrentScene->GetScene()->CreateScripts();

		if (RenderInstance::Get())
		{
			m_AvailableDevices = RenderInstance::Get()->GetRenderDevices();
			m_SelectedDeviceIndex = GetCurrentRenderDeviceDesc().ID;
			m_CurrentSwapChainSpec = GetCurrentSwapChainSepc();
		}

		BufferDescription desc = { sizeof(UniformBuffer), BufferType::Uniform, true, true };

		//const auto& cubeMapAsset = MainAssetManager.GetAsset<CubeMapAsset>("sky.hycube");
		//cubeMapAsset->Parse(cubeMapAsset->GetPath());
		//Skybox = cubeMapAsset->GetCubeMap();

		_BrowserPanel.Setup();
	}

	virtual void OnShutdown() override
	{
		_BrowserPanel.Shutdown();
		TextureCache.Clear();
		m_Renderer.reset();
	}

	virtual void OnUpdate(float deltaTime) override
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

		CalculateFPS(deltaTime);

		CurrentScene->GetScene()->RenderPhysicsDebug();

		if (m_IsSimulating)
			PhysicsUpdate(deltaTime);

		//Entity cameraEntity;
		//if (ViewportVisible && UpdateCamera(CurrentScene->GetScene(), cameraEntity, ViewportSize.x, ViewportSize.y))
		//{
		//	const auto& camPos = cameraEntity.GetComponent<TransformComponent>().GetPosition();
		//	ViewportRenderer->Render(CurrentScene->GetScene(), cameraEntity.GetComponent<CameraComponent>(), camPos, {}, Skybox);
		//	ViewportPostProcessing.PostProcessOffscreen(ViewportRenderer, ViewportSize.x, ViewportSize.y);
		//}

		//if (Input::IsMouseButtonDown(KeyCode::MouseRight) && IsHoveringSceneViewport())
		//{
		//	Viewport::ConfineCursor(
		//		SceneViewportPos.x,
		//		SceneViewportPos.x + SceneViewportSize.x,
		//		SceneViewportPos.y,
		//		SceneViewportPos.y + SceneViewportSize.y);
		//	FreeCam.Update(deltaTime);
		//}
		//else
		//{
		//	Viewport::ReleaseCursor();
		//}

		//if (SceneViewportVisible)
		//{
		//	FreeCam.CalculateView();
		//	UpdateCameraViewportSize(FreeCam, {
		//			(int)SceneViewportRenderer->GetWidth(),
		//			(int)SceneViewportRenderer->GetHeight()
		//		});
		//
		//	std::vector<Gizmo> gizmos;
		//	CurrentScene->GetScene()->IterateComponents(
		//		[&](Entity entity)
		//		{
		//			if (entity.HasComponent<CameraComponent>())
		//			{
		//				gizmos.push_back({ MainAssetManager.GetAsset<TextureAsset>("camera.png")->GetTexture(), entity.GetComponent<TransformComponent>().GetPosition(), {1, 1}});
		//			}
		//			else if (entity.HasComponent<PointLightComponent>())
		//			{
		//				gizmos.push_back({ MainAssetManager.GetAsset<TextureAsset>("point_light.png")->GetTexture(), entity.GetComponent<TransformComponent>().GetPosition(), {1, 1} });
		//			}
		//			else if (entity.HasComponent<DirectionalLightComponent>())
		//			{
		//				gizmos.push_back({ MainAssetManager.GetAsset<TextureAsset>("directional_light.png")->GetTexture(), entity.GetComponent<TransformComponent>().GetPosition(), {1, 1} });
		//			}
		//		});
		//
		//	SceneViewportRenderer->Render(CurrentScene->GetScene(), FreeCam, FreeCam.GetPosition(), gizmos, Skybox);
		//	//SceneViewportPostProcessing.PostProcessOffscreen(SceneViewportRenderer, SceneViewportSize.x, SceneViewportSize.y);
		//}

		RenderScene(deltaTime);
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

		DrawLogMessages();

		DrawSettingsWindow();

		_BrowserPanel.OnImGuiRender();
		_EditorPanel.OnImGuiRender();
		_SceneHierarchy.OnImGuiRender();

		_Inspector.SetContext(
			CurrentScene->GetScene(),
			_SceneHierarchy.GetSelectedEntity());

		_Inspector.OnImGuiRender();

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

		//RenderToolbar();
	}

	virtual void OnImGuiMenuBarRender() override
	{
		if (!ImGui::BeginMenuBar())
			return;

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Scene"))
				CurrentScene->Save();

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Game"))
		{
			if (ImGui::MenuItem(m_IsSimulating ? "Stop" : "Play"))
				ToggleSimulation();

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}

	//void RenderToolbar()
	//{
	//	ImGuiViewport* viewport = ImGui::GetMainViewport();
	//
	//	ImGui::SetNextWindowPos(
	//		ImVec2(ViewportPos.x, ViewportPos.y + 20));
	//
	//	ImGui::SetNextWindowViewport(viewport->ID);
	//
	//	ImGuiWindowFlags window_flags =
	//		ImGuiWindowFlags_NoDocking
	//		| ImGuiWindowFlags_NoTitleBar
	//		| ImGuiWindowFlags_NoResize
	//		| ImGuiWindowFlags_NoMove
	//		| ImGuiWindowFlags_NoScrollbar
	//		| ImGuiWindowFlags_NoSavedSettings;
	//
	//	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
	//	ImGui::Begin("TOOLBAR", nullptr, window_flags);
	//	ImGui::PopStyleVar();
	//
	//	if (ImGui::Button(m_IsSimulating ? "Stop" : "Play"))
	//		ToggleSimulation();
	//
	//	ImGui::SameLine();
	//
	//	if (ImGui::RadioButton("Translate",
	//		GuizmoTool == ImGuizmo::TRANSLATE))
	//		GuizmoTool = ImGuizmo::TRANSLATE;
	//
	//	ImGui::SameLine();
	//
	//	if (ImGui::RadioButton("Rotate",
	//		GuizmoTool == ImGuizmo::ROTATE))
	//		GuizmoTool = ImGuizmo::ROTATE;
	//
	//	ImGui::SameLine();
	//
	//	if (ImGui::RadioButton("Scale",
	//		GuizmoTool == ImGuizmo::SCALE))
	//		GuizmoTool = ImGuizmo::SCALE;
	//
	//	ImGui::End();
	//}

	virtual void OnSwapchainRecreation() override
	{
		TextureCache.Clear();
		m_Renderer->UpdateSwapChain(ActiveSwapChain.get());
	}

	virtual void OnRenderDeviceChangeStart() override
	{
		TextureCache.Clear();
		m_Renderer.reset();
	}

	virtual void OnRenderDeviceChangeFinish() override
	{
		BufferDescription desc = { sizeof(UniformBuffer), BufferType::Uniform, true, true };

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

		UniformBuffer cameraInfo = {};
		cameraInfo.View = camera.View;
		cameraInfo.Proj = camera.Proj;
		cameraInfo.ViewPos = cameraPos;

		bool viewportChanged = false;

		m_Renderer->Render(
			[this, cameraInfo, scene, &viewportChanged](RenderGraph* graph) -> const std::vector<DescriptorBindingValue>
			{
				if (m_ViewportContentRegion.x != m_OldViewportContentRegion.x || m_ViewportContentRegion.y != m_OldViewportContentRegion.y)
				{
					m_OldViewportContentRegion = m_ViewportContentRegion;
					viewportChanged = true;
				}

				RgTextureDesc finalSceneDesc = {};
				finalSceneDesc.Width = (uint32_t)m_ViewportContentRegion.x;
				finalSceneDesc.Height = (uint32_t)m_ViewportContentRegion.y;
				finalSceneDesc.Format = TextureFormat::RGBA8_SRGB;
				auto finalTexture = graph->CreateTexture(finalSceneDesc);

				auto swapChainImage = ActiveSwapChain->AcquireNextImage(graph, m_Renderer->GetImageAvailableSemaphore());
				VkSampler viewportSampler = m_Renderer->GetImguiSampler();
				auto wireframeMode = m_WireframeMode;

				graph->AddPass("Triangle", {},
					[finalTexture](RgPassBuilder& builder)
					{
						builder.WriteColor(finalTexture);
					},
					[scene, wireframeMode](RgCommandList& cmd)
					{
						auto vertexShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("TriangleVertexShader.glsl");
						auto fragmentShader = Application::Get()->MainAssetManager.GetAsset<ShaderAsset>("TriangleFragmentShader.glsl");

						PipelineSpec trianglePipeline = {};
						trianglePipeline.VertexBufferLayout = { { VertexElementType::Float3 }, { VertexElementType::Float2 }, { VertexElementType::Float3 }, { VertexElementType::Float3 } };
						trianglePipeline.ColorBlending = { BlendMode::None };
						trianglePipeline.PushConstants = { { sizeof(PushConstants), (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex) } };
						if (wireframeMode)
						{
							trianglePipeline.PolygonMode = PolygonModeStyle::Line;
						}
						cmd.BindPipeline(vertexShader, fragmentShader, trianglePipeline);

						scene->IterateComponents<MeshRendererComponent>([&cmd](Entity e, MeshRendererComponent mesh)
							{
								PushConstants pc = {};
								pc.Model = e.GetComponent<TransformComponent>().Transform;
								pc.Color = glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
								pc.TexIndex = 0;

								cmd.PushConstants(&pc, sizeof(PushConstants), 0, (ShaderStage)((uint32_t)ShaderStage::Fragment | (uint32_t)ShaderStage::Vertex));

								cmd.BindVertexBuffer(mesh.Mesh->GetVertexBuffer(Application::Get()->ActiveRenderDevice.get()));
								cmd.BindIndexBuffer(mesh.Mesh->GetIndexBuffer(Application::Get()->ActiveRenderDevice.get()));
								cmd.DrawIndexed(mesh.Mesh->GetIndexCount());
							});
					});

				auto& contentRegion = m_ViewportContentRegion;

				graph->AddPass("ImGui", {},
					[finalTexture, swapChainImage](RgPassBuilder& builder)
					{
						builder.WriteColor(swapChainImage);
						builder.ReadTexture(finalTexture);
					},
					[viewportSampler, finalSceneDesc, finalTexture, &contentRegion](RgCommandList& cmd)
					{
						ImGui::Begin("Viewport");
						contentRegion = ImGui::GetContentRegionAvail();
						ImGui::Image(TextureCache.GetTextureID(cmd.GetTextureView(finalTexture).ImageView, viewportSampler), ImVec2((float)finalSceneDesc.Width, (float)finalSceneDesc.Height));
						ImGui::End();

						ImGui::Render();
						ImDrawData* drawData = ImGui::GetDrawData();

						ImGui_ImplVulkan_RenderDrawData(drawData, cmd.GetCommandBuffer());
					});

				graph->Compile({ { 0, DescriptorType::UniformBuffer, 1, ShaderStage::Vertex } });

				return { { sizeof(UniformBuffer), (uint32_t*) &cameraInfo}};
			}, true);

		if (viewportChanged)
		{
			ActiveRenderDevice->WaitForIdle();
			m_Renderer->ClearCache();
			TextureCache.Clear();
		}
	}
};

extern std::shared_ptr<Application> GetApplication()
{
	return std::make_shared<EditorApp>();
}
