#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"

using namespace Hydrogen;

void Application::Run()
{
	OnSetup();

	EngineLogger::Init();
	AppLogger::Init();

	MainViewport = Viewport::Create(ApplicationSpec.ViewportTitle, (int)ApplicationSpec.ViewportSize.x, (int)ApplicationSpec.ViewportSize.y, (int)ApplicationSpec.ViewportPos.x, (int)ApplicationSpec.ViewportPos.y);
	MainViewport->Open();

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);

	OnStartup();
	while (MainViewport->IsOpen())
	{
		Viewport::PumpMessages();
		OnUpdate();
	}
	OnShutdown();
}
