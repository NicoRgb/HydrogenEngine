#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"

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

	MainAssetManager.LoadAssets("Assets");

	auto renderContext = RenderContext::Create(ApplicationSpec.Name, ApplicationSpec.Version, MainViewport);

	MainViewport->GetResizeEvent().AddListener(std::bind(&Application::OnResize, this, std::placeholders::_1, std::placeholders::_2));

	std::shared_ptr<Texture> texture = nullptr;

	if (ApplicationSpec.UseDebugGUI)
	{
		texture = Texture::Create(renderContext, TextureFormat::FormatR8G8B8A8, 1920, 1080);
	}

	auto renderPass = RenderPass::Create(renderContext, texture);
	auto pipeline = Pipeline::Create(renderContext, renderPass, MainAssetManager.GetAsset<ShaderAsset>("VertexShader.glsl"), MainAssetManager.GetAsset<ShaderAsset>("FragmentShader.glsl"),
		{ {VertexElementType::Float2}, {VertexElementType::Float3} });

	auto framebuffer = Framebuffer::Create(renderContext, renderPass);
	MainViewport->GetResizeEvent().AddListener([&framebuffer, &renderContext](int width, int height) { renderContext->OnResize(width, height); framebuffer->OnResize(width, height); });

	const std::vector<float> vertices = {
		-0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
		0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 0.0f, 1.0f,
		-0.5f, 0.5f, 1.0f, 1.0f, 1.0f
	};

	const std::vector<uint16_t> indices = {
		0, 1, 2, 2, 3, 0
	};

	auto vertexBuffer = VertexBuffer::Create(renderContext, { {VertexElementType::Float2}, {VertexElementType::Float3} }, (void*)vertices.data(), vertices.size() / 5);
	auto indexBuffer = IndexBuffer::Create(renderContext, indices);

	std::shared_ptr<RenderPass> renderPassDebugGUI = nullptr;
	std::shared_ptr<DebugGUI> debugGUI = nullptr;
	std::shared_ptr<Framebuffer> framebufferTexture = nullptr;

	Renderer MainRenderer(renderContext);
	Renderer ImGuiRenderer(renderContext);

	if (ApplicationSpec.UseDebugGUI)
	{
		renderPassDebugGUI = RenderPass::Create(renderContext);
		debugGUI = DebugGUI::Create(renderContext, renderPass);
		framebufferTexture = Framebuffer::Create(renderContext, renderPass, texture);
	}

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);

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

		MainRenderer.BeginFrame(debugGUI ? framebufferTexture : framebuffer);
		MainRenderer.Draw(vertexBuffer, indexBuffer, renderPass, pipeline);
		MainRenderer.EndFrame();

		if (debugGUI)
		{
			ImGuiRenderer.BeginFrame(framebuffer);
			ImGuiRenderer.DrawDebugGui(renderPassDebugGUI, debugGUI);
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
