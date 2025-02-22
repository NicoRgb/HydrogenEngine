#include "Hydrogen/Viewport.h"
#include "Hydrogen/Platform/Windows/WindowsViewport.h"

using namespace Hydrogen;

void Viewport::PumpMessages()
{
	return WindowsViewport::PumpMessages();
}

std::shared_ptr<Viewport> Viewport::Create(std::string name, int width, int height, int x, int y)
{
	return std::make_shared<WindowsViewport>(name, width, height, x, y);
}
