#pragma once

#include <Hydrogen/Hydrogen.hpp>

extern std::shared_ptr<Hydrogen::Application> GetApplication();

int main()
{
	Hydrogen::EngineLogger::Init();
	Hydrogen::AppLogger::Init();

	{
		auto app = GetApplication();
		app->Run();
	}

	Hydrogen::EngineLogger::Shutdown();
	Hydrogen::AppLogger::Shutdown();

	return 0;
}
