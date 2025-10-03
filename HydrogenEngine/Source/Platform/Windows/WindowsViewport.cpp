#ifdef HY_SYSTEM_WINDOWS

#define VK_USE_PLATFORM_WIN32_KHR

#include "Hydrogen/Platform/Windows/WindowsViewport.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Core.hpp"

#include "backends/imgui_impl_win32.h"

using namespace Hydrogen;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

class WindowsBackend
{
public:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		{
			return true;
		}

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

	RECT rect = { 0, 0, width, height };
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

	m_hWnd = CreateWindowEx(
		0,
		WindowsBackend::GetWindowClassName(),
		name.c_str(),
		WS_OVERLAPPEDWINDOW,
		x == 0 ? CW_USEDEFAULT : x,
		y == 0 ? CW_USEDEFAULT : y,
		width == 0 ? CW_USEDEFAULT : rect.right - rect.left,
		height == 0 ? CW_USEDEFAULT : rect.bottom - rect.top,
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

const std::vector<const char*> WindowsViewport::GetVulkanExtensions() const
{
	return std::vector<const char*> { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
}

void* WindowsViewport::CreateVulkanSurface(const RenderContext* renderContext) const
{
	VkSurfaceKHR surface;

	VkWin32SurfaceCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hwnd = m_hWnd;
	createInfo.hinstance = WindowsBackend::GetHInstance();

	const VulkanRenderContext* vkContext = dynamic_cast<const VulkanRenderContext*>(renderContext);

	HY_ASSERT(vkCreateWin32SurfaceKHR(vkContext->GetInstance(), &createInfo, nullptr, &surface) == VK_SUCCESS, "Failed to create win32 window surface for vulkan");

	return (void*)surface;
}

void WindowsViewport::ImGuiNewFrame() const
{
	ImGui_ImplWin32_NewFrame();
}

void WindowsViewport::InitImGui() const
{
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui::GetPlatformIO().Platform_CreateVkSurface = [](ImGuiViewport* viewport, ImU64 vk_instance, const void* vk_allocator, ImU64* out_vk_surface) -> int
		{
			VkWin32SurfaceCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
			createInfo.hwnd = (HWND)viewport->PlatformHandleRaw;
			createInfo.hinstance = ::GetModuleHandle(nullptr);
			return (int)vkCreateWin32SurfaceKHR((VkInstance)vk_instance, &createInfo, (VkAllocationCallbacks*)vk_allocator, (VkSurfaceKHR*)out_vk_surface);
		};
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

	case WM_SIZE:
		m_Width = (int)LOWORD(lParam);
		m_Height = (int)HIWORD(lParam);
		m_ResizeEvent.Invoke(m_Width, m_Height);
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#endif
