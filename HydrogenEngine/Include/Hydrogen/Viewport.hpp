#pragma once

#include <string>
#include <memory>

#include "Event.hpp"

namespace Hydrogen
{
	class Viewport
	{
	public:
		~Viewport() = default;

		virtual void Open() = 0;
		virtual void Close() = 0;

		virtual int GetWidth() const = 0;
		virtual int GetHeight() const = 0;
		virtual int IsOpen() const = 0;

		static void PumpMessages();
		static std::shared_ptr<Viewport> Create(std::string name, int width = 0, int height = 0, int x = 0, int y = 0);

		Event<int, int>& GetResizeEvent() { return m_ResizeEvent; }

	protected:
		Event<int, int> m_ResizeEvent;
	};
}
