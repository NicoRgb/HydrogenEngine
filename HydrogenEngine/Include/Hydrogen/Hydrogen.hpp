#pragma once

#include <Hydrogen/Application.hpp>
#include <memory>

extern std::shared_ptr<Hydrogen::Application> GetApplication();

int main(void)
{
	auto app = GetApplication();
	app->Run();
}
