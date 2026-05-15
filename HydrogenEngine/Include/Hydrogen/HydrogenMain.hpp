#pragma once

#include <Hydrogen/Core.hpp>
#include <Hydrogen/Application.hpp>
#include <Hydrogen/Logger.hpp>
#include <Hydrogen/Viewport.hpp>
#include <Hydrogen/AssetManager.hpp>
#include <Hydrogen/Renderer/Renderer.hpp>
#include <Hydrogen/Scene.hpp>
#include <Hydrogen/Physics.hpp>
#include <Hydrogen/Camera.hpp>
#include <Hydrogen/Input.hpp>
#include <Hydrogen/Renderer/DeferredRenderer.hpp>

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
