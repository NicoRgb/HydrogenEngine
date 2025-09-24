#pragma once

#include <string>
#include <memory>

#include "Event.hpp"

namespace Hydrogen
{
	class Viewport
	{
	public:
		virtual ~Viewport() = default;

		virtual void Open() = 0;
		virtual void Close() = 0;

		virtual int GetWidth() const = 0;
		virtual int GetHeight() const = 0;
		virtual int IsOpen() const = 0;

		virtual const std::vector<const char*> GetVulkanExtensions() const = 0;
		virtual void* CreateVulkanSurface(const class RenderContext* renderContext) const = 0;
		virtual void InitImGui() const = 0;
		virtual void ImGuiNewFrame() const = 0;

		static void PumpMessages();
		Event<int, int>& GetResizeEvent() { return m_ResizeEvent; }

		static std::shared_ptr<Viewport> Create(std::string name, int width = 0, int height = 0, int x = 0, int y = 0);

	protected:
		Event<int, int> m_ResizeEvent;
	};
}
