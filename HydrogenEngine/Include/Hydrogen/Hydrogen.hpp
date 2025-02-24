#pragma once

#include <Hydrogen/Core.hpp>
#include <Hydrogen/Application.hpp>
#include <Hydrogen/Logger.hpp>
#include <Hydrogen/Viewport.hpp>

extern std::shared_ptr<Hydrogen::Application> GetApplication();

int main(void)
{
	auto app = GetApplication();
	app->Run();
}
