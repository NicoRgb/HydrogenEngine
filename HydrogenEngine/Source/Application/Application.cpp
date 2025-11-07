#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"

using namespace Hydrogen;

Application* Application::s_Instance;

void Application::OnResize(int width, int height)
{
}

void Application::Run()
{
	OnSetup();

	EngineLogger::Init();
	AppLogger::Init();

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);

	MainViewport = Viewport::Create(ApplicationSpec.ViewportTitle, (int)ApplicationSpec.ViewportSize.x, (int)ApplicationSpec.ViewportSize.y, (int)ApplicationSpec.ViewportPos.x, (int)ApplicationSpec.ViewportPos.y);
	MainViewport->GetResizeEvent().AddListener(std::bind(&Application::OnResize, this, std::placeholders::_1, std::placeholders::_2));
	MainViewport->Open();

	CurrentScene = std::make_shared<Scene>();

	_RenderContext = RenderContext::Create(ApplicationSpec.Name, ApplicationSpec.Version, MainViewport);
	MainAssetManager.LoadAssets("Assets", _RenderContext);

	std::shared_ptr<Texture> texture = nullptr;

	if (ApplicationSpec.UseDebugGUI)
	{
		texture = Texture::Create(_RenderContext, TextureFormat::ViewportDefault, 1920, 1080);
	}

	_RenderPass = RenderPass::Create(_RenderContext);

	auto framebuffer = Framebuffer::Create(_RenderContext, _RenderPass);
	MainViewport->GetResizeEvent().AddListener([&framebuffer, this](int width, int height) { _RenderContext->OnResize(width, height); framebuffer->OnResize(width, height); });

	std::shared_ptr<RenderPass> renderPassTexture = nullptr;
	std::shared_ptr<DebugGUI> debugGUI = nullptr;
	std::shared_ptr<Framebuffer> framebufferTexture = nullptr;

	Renderer MainRenderer(_RenderContext);
	Renderer ImGuiRenderer(_RenderContext);

	if (ApplicationSpec.UseDebugGUI)
	{
		renderPassTexture = RenderPass::Create(_RenderContext, texture);
		debugGUI = DebugGUI::Create(_RenderContext, _RenderPass);
		framebufferTexture = Framebuffer::Create(_RenderContext, renderPassTexture, texture);
	}

	MainPipeline = MainRenderer.CreatePipeline(_RenderPass, MainAssetManager.GetAsset<ShaderAsset>("VertexShader.glsl"), MainAssetManager.GetAsset<ShaderAsset>("FragmentShader.glsl"));

	auto vikingRoomTextureAsset = MainAssetManager.GetAsset<TextureAsset>("viking_room_texture.png");
	auto vikingRoom = MainAssetManager.GetAsset<MeshAsset>("viking_room.obj");
	auto cube = MainAssetManager.GetAsset<MeshAsset>("cube.obj");

	Entity e0(CurrentScene, "Cube");
	e0.AddComponent<MeshRendererComponent>(vikingRoomTextureAsset, cube);

	Entity e1(CurrentScene, "Viking Room");
	e1.AddComponent<MeshRendererComponent>(vikingRoomTextureAsset, vikingRoom);

	OnStartup();
	while (MainViewport->IsOpen())
	{
		Viewport::PumpMessages();
		OnUpdate();

		if (MainViewport->GetWidth() == 0 || MainViewport->GetHeight() == 0)
		{
			continue;
		}

		if (debugGUI)
		{
			debugGUI->BeginFrame();
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

			ImGui::Begin("DockSpace Demo", &dockingEnabled, window_flags);
			ImGui::PopStyleVar(2);

			ImGuiID dockspace_id = ImGui::GetID("DockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

			ImGui::End();

			ImGui::Begin("Viewport");
			ImVec2 contentRegion = ImGui::GetContentRegionAvail();
			if (contentRegion.x != m_ViewportSize.x || contentRegion.y != m_ViewportSize.y)
			{
				m_ViewportSize = contentRegion;

				texture->Resize((size_t)contentRegion.x, (size_t)contentRegion.y);
				framebufferTexture->OnResize((int)contentRegion.x, (int)contentRegion.y);
			}

			ImGui::Image(texture->GetImGuiImage(), contentRegion);
			ImGui::End();

			OnImGuiRender();
			debugGUI->EndFrame();
		}

		MainRenderer.BeginFrame(debugGUI ? framebufferTexture : framebuffer, debugGUI ? renderPassTexture : _RenderPass,
			debugGUI ? texture->GetWidth() : MainViewport->GetWidth(),
			debugGUI ? texture->GetHeight() : MainViewport->GetHeight());

		CurrentScene->IterateComponents<TransformComponent, MeshRendererComponent>([&](Entity entity, const TransformComponent& transform, const MeshRendererComponent& mesh)
			{
				(void)entity;
				MainRenderer.Draw(mesh, MainPipeline, transform.Transform);
			});

		MainRenderer.EndFrame();

		if (debugGUI)
		{
			ImGuiRenderer.BeginFrame(framebuffer, _RenderPass, MainViewport->GetWidth(), MainViewport->GetHeight());
			ImGuiRenderer.DrawDebugGui(debugGUI);
			ImGuiRenderer.EndFrame();

			auto& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		}
	}

	OnShutdown();
}

void Application::ReloadShader()
{
}
