#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"
#include "imgui.h"

#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"

using namespace Hydrogen;

void Application::OnResize(int width, int height)
{
}

void Application::Run()
{
	OnSetup();

	EngineLogger::Init();
	AppLogger::Init();

	MainViewport = Viewport::Create(ApplicationSpec.ViewportTitle, (int)ApplicationSpec.ViewportSize.x, (int)ApplicationSpec.ViewportSize.y, (int)ApplicationSpec.ViewportPos.x, (int)ApplicationSpec.ViewportPos.y);
	MainViewport->Open();

	AssetManager assetManager;
	assetManager.LoadAssets("Assets");

	auto renderContext = RenderContext::Create(ApplicationSpec.Name, ApplicationSpec.Version, MainViewport);
	Renderer::Init(renderContext);

	MainViewport->GetResizeEvent().AddListener(std::bind(&Application::OnResize, this, std::placeholders::_1, std::placeholders::_2));

	auto pipeline = Pipeline::Create(renderContext, assetManager.GetAsset<ShaderAsset>("VertexShader.glsl"), assetManager.GetAsset<ShaderAsset>("FragmentShader.glsl"));
	auto framebuffer = Framebuffer::Create(renderContext, pipeline, false);
	MainViewport->GetResizeEvent().AddListener([&framebuffer](int width, int height) { framebuffer->OnResize(width, height); });

	std::shared_ptr<DebugGUI> debugGUI = nullptr;
	if (ApplicationSpec.UseDebugGUI)
	{
		debugGUI = DebugGUI::Create(renderContext, pipeline);
	}

	auto framebufferTexture = Framebuffer::Create(renderContext, pipeline, true);

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);

	const auto& images = Framebuffer::Get<VulkanFramebuffer>(framebufferTexture)->GetImGuiTextures();

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

			ImGui::Begin("Scene");
			ImGui::Image((ImTextureID)images[0], ImVec2(ApplicationSpec.ViewportSize.x, ApplicationSpec.ViewportSize.y));
			ImGui::End();

			OnImGuiRender();
			debugGUI->EndFrame();
		}

		Renderer::BeginFrame();
			Renderer::Draw(pipeline, framebufferTexture);
			if (debugGUI)
			{
				Renderer::DrawDebugGui(pipeline, framebuffer, debugGUI);
			}
		Renderer::EndFrame();

		if (debugGUI)
		{
			auto& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		}
	}
	OnShutdown();

	Renderer::Shutdown();
}
