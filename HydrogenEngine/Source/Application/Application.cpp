#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"

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
	auto framebuffer = Framebuffer::Create(renderContext, pipeline);
	MainViewport->GetResizeEvent().AddListener([&framebuffer](int width, int height) { framebuffer->OnResize(width, height); });

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

		Renderer::BeginFrame();
		Renderer::Draw(pipeline, framebuffer);
		Renderer::EndFrame();
	}
	OnShutdown();

	Renderer::Shutdown();
}
