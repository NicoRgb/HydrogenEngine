#define VK_USE_PLATFORM_WIN32_KHR

#include "Hydrogen/Platform/Windows/WindowsViewport.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Core.hpp"
#include "Hydrogen/Input.hpp"

#include "backends/imgui_impl_win32.h"

#include "windowsx.h"

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

	m_Rect = { 0, 0, width, height };
	AdjustWindowRect(&m_Rect, WS_OVERLAPPEDWINDOW, FALSE);

	m_hWnd = CreateWindowEx(
		0,
		WindowsBackend::GetWindowClassName(),
		name.c_str(),
		WS_OVERLAPPEDWINDOW,
		x == 0 ? CW_USEDEFAULT : x,
		y == 0 ? CW_USEDEFAULT : y,
		width == 0 ? CW_USEDEFAULT : m_Rect.right - m_Rect.left,
		height == 0 ? CW_USEDEFAULT : m_Rect.bottom - m_Rect.top,
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

void WindowsViewport::LockCursor()
{
	POINT center;
	center.x = (m_Rect.right - m_Rect.left) / 2;
	center.y = (m_Rect.bottom - m_Rect.top) / 2;

	ClientToScreen(m_hWnd, &center);
	SetCursorPos(center.x, center.y);
}

void WindowsViewport::UnlockCursor()
{
	ClipCursor(nullptr);
	ShowCursor(TRUE);
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

void Viewport::ViewportShowCursor()
{
	ShowCursor(TRUE);
}

void Viewport::ViewportHideCursor()
{
	ShowCursor(FALSE);
}

inline KeyCode WinKeyToKeyCode(WPARAM vk)
{
	switch (vk)
	{
	case 'A': return KeyCode::A;
	case 'B': return KeyCode::B;
	case 'C': return KeyCode::C;
	case 'D': return KeyCode::D;
	case 'E': return KeyCode::E;
	case 'F': return KeyCode::F;
	case 'G': return KeyCode::G;
	case 'H': return KeyCode::H;
	case 'I': return KeyCode::I;
	case 'J': return KeyCode::J;
	case 'K': return KeyCode::K;
	case 'L': return KeyCode::L;
	case 'M': return KeyCode::M;
	case 'N': return KeyCode::N;
	case 'O': return KeyCode::O;
	case 'P': return KeyCode::P;
	case 'Q': return KeyCode::Q;
	case 'R': return KeyCode::R;
	case 'S': return KeyCode::S;
	case 'T': return KeyCode::T;
	case 'U': return KeyCode::U;
	case 'V': return KeyCode::V;
	case 'W': return KeyCode::W;
	case 'X': return KeyCode::X;
	case 'Y': return KeyCode::Y;
	case 'Z': return KeyCode::Z;

	case '0': return KeyCode::Num0;
	case '1': return KeyCode::Num1;
	case '2': return KeyCode::Num2;
	case '3': return KeyCode::Num3;
	case '4': return KeyCode::Num4;
	case '5': return KeyCode::Num5;
	case '6': return KeyCode::Num6;
	case '7': return KeyCode::Num7;
	case '8': return KeyCode::Num8;
	case '9': return KeyCode::Num9;

	case VK_F1:  return KeyCode::F1;
	case VK_F2:  return KeyCode::F2;
	case VK_F3:  return KeyCode::F3;
	case VK_F4:  return KeyCode::F4;
	case VK_F5:  return KeyCode::F5;
	case VK_F6:  return KeyCode::F6;
	case VK_F7:  return KeyCode::F7;
	case VK_F8:  return KeyCode::F8;
	case VK_F9:  return KeyCode::F9;
	case VK_F10: return KeyCode::F10;
	case VK_F11: return KeyCode::F11;
	case VK_F12: return KeyCode::F12;

	case VK_UP:    return KeyCode::Up;
	case VK_DOWN:  return KeyCode::Down;
	case VK_LEFT:  return KeyCode::Left;
	case VK_RIGHT: return KeyCode::Right;
	case VK_HOME:  return KeyCode::Home;
	case VK_END:   return KeyCode::End;
	case VK_PRIOR: return KeyCode::PageUp;
	case VK_NEXT:  return KeyCode::PageDown;
	case VK_INSERT:return KeyCode::Insert;
	case VK_DELETE:return KeyCode::Delete;

	case VK_SHIFT:
	case VK_LSHIFT: return KeyCode::LeftShift;
	case VK_RSHIFT: return KeyCode::RightShift;

	case VK_CONTROL:
	case VK_LCONTROL: return KeyCode::LeftControl;
	case VK_RCONTROL: return KeyCode::RightControl;

	case VK_MENU:
	case VK_LMENU: return KeyCode::LeftAlt;
	case VK_RMENU: return KeyCode::RightAlt;

	case VK_LWIN: return KeyCode::LeftSuper;
	case VK_RWIN: return KeyCode::RightSuper;

	case VK_CAPITAL: return KeyCode::CapsLock;
	case VK_NUMLOCK: return KeyCode::NumLock;
	case VK_SCROLL:  return KeyCode::ScrollLock;

	case VK_ESCAPE:    return KeyCode::Escape;
	case VK_RETURN:    return KeyCode::Enter;
	case VK_TAB:       return KeyCode::Tab;
	case VK_BACK:      return KeyCode::Backspace;
	case VK_SPACE:     return KeyCode::Space;
	case VK_APPS:      return KeyCode::Menu;
	case VK_PRINT:     return KeyCode::PrintScreen;
	case VK_PAUSE:     return KeyCode::Pause;

	case VK_NUMPAD0: return KeyCode::KP_0;
	case VK_NUMPAD1: return KeyCode::KP_1;
	case VK_NUMPAD2: return KeyCode::KP_2;
	case VK_NUMPAD3: return KeyCode::KP_3;
	case VK_NUMPAD4: return KeyCode::KP_4;
	case VK_NUMPAD5: return KeyCode::KP_5;
	case VK_NUMPAD6: return KeyCode::KP_6;
	case VK_NUMPAD7: return KeyCode::KP_7;
	case VK_NUMPAD8: return KeyCode::KP_8;
	case VK_NUMPAD9: return KeyCode::KP_9;
	case VK_DECIMAL:  return KeyCode::KP_Decimal;
	case VK_DIVIDE:   return KeyCode::KP_Divide;
	case VK_MULTIPLY: return KeyCode::KP_Multiply;
	case VK_SUBTRACT: return KeyCode::KP_Subtract;
	case VK_ADD:      return KeyCode::KP_Add;
	case VK_SEPARATOR:return KeyCode::KP_Enter;

	case VK_LBUTTON: return KeyCode::MouseLeft;
	case VK_RBUTTON: return KeyCode::MouseRight;
	case VK_MBUTTON: return KeyCode::MouseMiddle;
	case VK_XBUTTON1:return KeyCode::MouseButton4;
	case VK_XBUTTON2:return KeyCode::MouseButton5;

	case VK_OEM_1:      return KeyCode::Semicolon;    // ; :
	case VK_OEM_PLUS:   return KeyCode::Equal;        // = +
	case VK_OEM_COMMA:  return KeyCode::Comma;        // , <
	case VK_OEM_MINUS:  return KeyCode::Minus;        // - _
	case VK_OEM_PERIOD: return KeyCode::Period;       // . >
	case VK_OEM_2:      return KeyCode::Slash;        // / ?
	case VK_OEM_3:      return KeyCode::GraveAccent;  // ` ~
	case VK_OEM_4:      return KeyCode::LeftBracket;  // [ {
	case VK_OEM_5:      return KeyCode::Backslash;    // \ |
	case VK_OEM_6:      return KeyCode::RightBracket; // ] }
	case VK_OEM_7:      return KeyCode::Apostrophe;   // ' "

	default:
		return KeyCode::Unknown;
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

	// Input
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		KeyCode key = WinKeyToKeyCode(wParam);
		if (key != KeyCode::Unknown)
		{
			Input::OnKeyDown.Invoke(key);
			Input::OnKey.Invoke(key);
		}
		return 0;
	}

	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		KeyCode key = WinKeyToKeyCode(wParam);
		if (key != KeyCode::Unknown)
		{
			Input::OnKeyUp.Invoke(key);
			Input::OnKey.Invoke(key);
		}
		return 0;
	}

	case WM_LBUTTONDOWN:
	{
		Input::OnMouseButtonDown.Invoke(KeyCode::MouseLeft);
		Input::OnMouseButton.Invoke(KeyCode::MouseLeft);
		return 0;
	}

	case WM_LBUTTONUP:
	{
		Input::OnMouseButtonUp.Invoke(KeyCode::MouseLeft);
		Input::OnMouseButton.Invoke(KeyCode::MouseLeft);
		return 0;
	}

	case WM_RBUTTONDOWN:
	{
		Input::OnMouseButtonDown.Invoke(KeyCode::MouseRight);
		Input::OnMouseButton.Invoke(KeyCode::MouseRight);
		return 0;
	}

	case WM_RBUTTONUP:
	{
		Input::OnMouseButtonUp.Invoke(KeyCode::MouseRight);
		Input::OnMouseButton.Invoke(KeyCode::MouseRight);
		return 0;
	}

	case WM_MBUTTONDOWN:
	{
		Input::OnMouseButtonDown.Invoke(KeyCode::MouseMiddle);
		Input::OnMouseButton.Invoke(KeyCode::MouseMiddle);
		return 0;
	}

	case WM_MBUTTONUP:
	{
		Input::OnMouseButtonUp.Invoke(KeyCode::MouseMiddle);
		Input::OnMouseButton.Invoke(KeyCode::MouseMiddle);
		return 0;
	}

	case WM_XBUTTONDOWN:
	{
		KeyCode key = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
			? KeyCode::MouseButton4
			: KeyCode::MouseButton5;

		Input::OnMouseButtonDown.Invoke(key);
		Input::OnMouseButton.Invoke(key);
		return TRUE;
	}

	case WM_XBUTTONUP:
	{
		KeyCode key = (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
			? KeyCode::MouseButton4
			: KeyCode::MouseButton5;

		Input::OnMouseButtonUp.Invoke(key);
		Input::OnMouseButton.Invoke(key);
		return TRUE;
	}

	case WM_MOUSEMOVE:
	{
		float x = static_cast<float>(GET_X_LPARAM(lParam));
		float y = static_cast<float>(GET_Y_LPARAM(lParam));
		Input::OnMouseMove.Invoke(x, y);
		return 0;
	}

	default:
	{
	}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
