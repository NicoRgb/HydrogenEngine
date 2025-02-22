#include "Hydrogen/Platform/Windows/WindowsViewport.h"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

class WindowsBackend
{
public:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (uMsg == WM_NCCREATE)
		{
			CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
			WindowsViewport* pWindow = static_cast<WindowsViewport*>(pCreate->lpCreateParams);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWindow));

			return pWindow->WindowProc(hWnd, uMsg, wParam, lParam);
		}

		WindowsViewport* pWindow = reinterpret_cast<WindowsViewport*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
		if (pWindow)
		{
			return pWindow->WindowProc(hWnd, uMsg, wParam, lParam);
		}

		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	static void RegisterWindowClass()
	{
		s_hInstance = GetModuleHandle(NULL);

		WNDCLASS wc = {};
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = s_hInstance;
		wc.lpszClassName = CLASS_NAME;

		RegisterClass(&wc);
	}

	static const char* GetWindowClassName() { return CLASS_NAME; }
	static const HINSTANCE GetHInstance() { return s_hInstance; }

private:
	static constexpr char CLASS_NAME[] = "HydrogenEngineWindowClass";
	static HINSTANCE s_hInstance;
};

HINSTANCE WindowsBackend::s_hInstance;

WindowsViewport::WindowsViewport(std::string name, int width, int height, int x, int y)
	: m_Width(width), m_Height(height), m_IsOpen(false)
{
	WindowsBackend::RegisterWindowClass();

	m_hWnd = CreateWindowEx(
		0,
		WindowsBackend::GetWindowClassName(),
		name.c_str(),
		WS_OVERLAPPEDWINDOW,
		x == 0 ? CW_USEDEFAULT : x,
		y == 0 ? CW_USEDEFAULT : y,
		width == 0 ? CW_USEDEFAULT : width,
		height == 0 ? CW_USEDEFAULT : height,
		NULL,
		NULL,
		WindowsBackend::GetHInstance(),
		this
	);

	HY_ASSERT(m_hWnd, "Failed to create hWnd");
}

void WindowsViewport::Open()
{
	ShowWindow(m_hWnd, 1);
	m_IsOpen = true;
}

void WindowsViewport::Close()
{
	m_IsOpen = false;
}

void WindowsViewport::PumpMessages()
{
	MSG msg = { };
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)
	{
		TranslateMessage(&msg);
		if (msg.message == WM_QUIT)
		{
			HY_ENGINE_INFO("WM_QUIT message received");
			exit((int)msg.wParam);
		}

		DispatchMessage(&msg);
	}
}

LRESULT WindowsViewport::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		Close();
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
