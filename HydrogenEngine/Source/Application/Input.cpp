#include "Hydrogen/Input.hpp"

using namespace Hydrogen;

Event<KeyCode> Input::OnKeyDown;
Event<KeyCode> Input::OnKeyUp;
Event<KeyCode> Input::OnKey;
Event<KeyCode> Input::OnMouseButtonDown;
Event<KeyCode> Input::OnMouseButtonUp;
Event<KeyCode> Input::OnMouseButton;
Event<float, float> Input::OnMouseMove;
