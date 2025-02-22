#pragma once

#include "Hydrogen/Viewport.h"

#include <Windows.h>

namespace Hydrogen
{
	class WindowsViewport : public Viewport
	{
	public:
		WindowsViewport(std::string name, int width, int height, int x, int y);
		~WindowsViewport() = default;

		void Open() override;
		void Close() override;

		int GetWidth() const override { return m_Width; }
		int GetHeight() const override { return m_Height; }
		int IsOpen() const override { return m_IsOpen; }

		static void PumpMessages();
		LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	private:
		int m_Width, m_Height;
		bool m_IsOpen;

		HWND m_hWnd;
	};
}
