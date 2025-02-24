#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"

#include "Hydrogen/Renderer/RenderContext.hpp"

using namespace Hydrogen;

void Test(int width, int height)
{
	HY_ENGINE_INFO("Resize: {}, {}", width, height);
}

void Application::Run()
{
	OnSetup();

	EngineLogger::Init();
	AppLogger::Init();

	MainViewport = Viewport::Create(ApplicationSpec.ViewportTitle, (int)ApplicationSpec.ViewportSize.x, (int)ApplicationSpec.ViewportSize.y, (int)ApplicationSpec.ViewportPos.x, (int)ApplicationSpec.ViewportPos.y);
	MainViewport->Open();
	MainViewport->GetResizeEvent().AddListener(Test);

	auto renderContext = RenderContext::Create(ApplicationSpec.Name, ApplicationSpec.Version);

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);

	OnStartup();
	while (MainViewport->IsOpen())
	{
		Viewport::PumpMessages();
		OnUpdate();
	}
	OnShutdown();
}
