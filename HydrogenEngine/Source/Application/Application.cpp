#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"

using namespace Hydrogen;

void Application::Run()
{
	OnSetup();

	EngineLogger::Init();
	AppLogger::Init();

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);

	OnStartup();
	while (true)
	{
		OnUpdate();
	}
	OnShutdown();
}
