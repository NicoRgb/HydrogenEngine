#pragma once

#include "Hydrogen/Event.hpp"
#include <unordered_map>

namespace Hydrogen
{
	enum class KeyCode
	{
		Unknown = 0,

		A, B, C, D, E, F, G,
		H, I, J, K, L, M, N,
		O, P, Q, R, S, T, U,
		V, W, X, Y, Z,

		Num0, Num1, Num2, Num3, Num4,
		Num5, Num6, Num7, Num8, Num9,

		F1, F2, F3, F4, F5, F6,
		F7, F8, F9, F10, F11, F12,
		F13, F14, F15, F16, F17, F18,
		F19, F20, F21, F22, F23, F24,

		KP_0, KP_1, KP_2, KP_3, KP_4,
		KP_5, KP_6, KP_7, KP_8, KP_9,
		KP_Decimal, KP_Divide, KP_Multiply,
		KP_Subtract, KP_Add, KP_Enter, KP_Equal,

		LeftShift, RightShift,
		LeftControl, RightControl,
		LeftAlt, RightAlt,
		LeftSuper, RightSuper,  // Windows / Command key
		CapsLock, NumLock, ScrollLock,

		Up, Down, Left, Right,
		PageUp, PageDown,
		Home, End,
		Insert, Delete,
		PrintScreen, Pause,

		Space,
		Apostrophe,     // '
		Comma,          // ,
		Minus,          // -
		Period,         // .
		Slash,          // /
		Semicolon,      // ;
		Equal,          // =
		LeftBracket,    // [
		Backslash,      // '\'
		RightBracket,   // ]
		GraveAccent,    // `

		Escape,
		Enter,
		Tab,
		Backspace,
		Menu,

		VolumeUp,
		VolumeDown,
		VolumeMute,
		MediaNext,
		MediaPrevious,
		MediaStop,
		MediaPlayPause,

		MouseLeft,
		MouseRight,
		MouseMiddle,
		MouseButton4,
		MouseButton5,
		MouseButton6,
		MouseButton7,
		MouseButton8,

		MouseWheelUp,
		MouseWheelDown,

		GamepadA,
		GamepadB,
		GamepadX,
		GamepadY,
		GamepadLeftBumper,
		GamepadRightBumper,
		GamepadBack,
		GamepadStart,
		GamepadGuide,
		GamepadLeftStick,
		GamepadRightStick,
		GamepadDPadUp,
		GamepadDPadDown,
		GamepadDPadLeft,
		GamepadDPadRight,
		GamepadLeftTrigger,
		GamepadRightTrigger,
	};

	class Input
	{
	public:
		static Event<KeyCode> OnKeyDown;
		static Event<KeyCode> OnKeyUp;
		static Event<KeyCode> OnKey;
		static Event<KeyCode> OnMouseButtonDown;
		static Event<KeyCode> OnMouseButtonUp;
		static Event<KeyCode> OnMouseButton;
		static Event<float, float> OnMouseMove;

		static void Initialize()
		{
			OnKeyDown.AddListener([](KeyCode key)
				{
				s_KeyStates[key] = true;
				});

			OnKeyUp.AddListener([](KeyCode key)
				{
				s_KeyStates[key] = false;
				});

			OnMouseButtonDown.AddListener([](KeyCode button)
				{
				s_MouseStates[button] = true;
				});

			OnMouseButtonUp.AddListener([](KeyCode button)
				{
				s_MouseStates[button] = false;
				});

			OnMouseMove.AddListener([](float x, float y)
				{
				s_MouseDeltaX = x - s_MouseX;
				s_MouseDeltaY = y - s_MouseY;

				s_MouseX = x;
				s_MouseY = y;
				});
		}

		static bool IsKeyDown(KeyCode key)
		{
			auto it = s_KeyStates.find(key);
			return it != s_KeyStates.end() && it->second;
		}

		static bool IsMouseButtonDown(KeyCode button)
		{
			auto it = s_MouseStates.find(button);
			return it != s_MouseStates.end() && it->second;
		}

		static float GetMouseX() { return s_MouseX; }
		static float GetMouseY() { return s_MouseY; }

		static float GetMouseDeltaX() { return s_MouseDeltaX; }
		static float GetMouseDeltaY() { return s_MouseDeltaY; }

		static void EndFrame()
		{
			s_MouseDeltaX = 0.0f;
			s_MouseDeltaY = 0.0f;
		}

	private:
		static inline std::unordered_map<KeyCode, bool> s_KeyStates{};
		static inline std::unordered_map<KeyCode, bool> s_MouseStates{};

		static inline float s_MouseX = 0.0f;
		static inline float s_MouseY = 0.0f;
		static inline float s_MouseDeltaX = 0.0f;
		static inline float s_MouseDeltaY = 0.0f;
	};
}
