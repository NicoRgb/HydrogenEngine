#include "Hydrogen/Viewport.hpp"
#include "Hydrogen/Platform/Windows/WindowsViewport.hpp"

using namespace Hydrogen;

void Viewport::PumpMessages()
{
#ifdef HY_SYSTEM_WINDOWS
    return WindowsViewport::PumpMessages();
#endif
}

std::shared_ptr<Viewport> Viewport::Create(std::string name, int width, int height, int x, int y)
{
#ifdef HY_SYSTEM_WINDOWS
	return std::make_shared<WindowsViewport>(name, width, height, x, y);
#else
    return nullptr;
#endif
}
