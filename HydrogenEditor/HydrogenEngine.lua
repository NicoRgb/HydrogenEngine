-- ==========================================
-- AUTO-GENERATED HYDROGEN ENGINE LUA STUBS
-- ==========================================

---@class BodyType
---@field STATIC integer
---@field KINEMATIC integer
---@field DYNAMIC integer
BodyType = {}

---@class Key
---@field Unknown integer
---@field A integer
---@field B integer
---@field C integer
---@field D integer
---@field E integer
---@field F integer
---@field G integer
---@field H integer
---@field I integer
---@field J integer
---@field K integer
---@field L integer
---@field M integer
---@field N integer
---@field O integer
---@field P integer
---@field Q integer
---@field R integer
---@field S integer
---@field T integer
---@field U integer
---@field V integer
---@field W integer
---@field X integer
---@field Y integer
---@field Z integer
---@field Num0 integer
---@field Num1 integer
---@field Num2 integer
---@field Num3 integer
---@field Num4 integer
---@field Num5 integer
---@field Num6 integer
---@field Num7 integer
---@field Num8 integer
---@field Num9 integer
---@field F1 integer
---@field F2 integer
---@field F3 integer
---@field F4 integer
---@field F5 integer
---@field F6 integer
---@field F7 integer
---@field F8 integer
---@field F9 integer
---@field F10 integer
---@field F11 integer
---@field F12 integer
---@field F13 integer
---@field F14 integer
---@field F15 integer
---@field F16 integer
---@field F17 integer
---@field F18 integer
---@field F19 integer
---@field F20 integer
---@field F21 integer
---@field F22 integer
---@field F23 integer
---@field F24 integer
---@field KP_0 integer
---@field KP_1 integer
---@field KP_2 integer
---@field KP_3 integer
---@field KP_4 integer
---@field KP_5 integer
---@field KP_6 integer
---@field KP_7 integer
---@field KP_8 integer
---@field KP_9 integer
---@field KP_Decimal integer
---@field KP_Divide integer
---@field KP_Multiply integer
---@field KP_Subtract integer
---@field KP_Add integer
---@field KP_Enter integer
---@field KP_Equal integer
---@field LeftShift integer
---@field RightShift integer
---@field LeftControl integer
---@field RightControl integer
---@field LeftAlt integer
---@field RightAlt integer
---@field LeftSuper integer
---@field RightSuper integer
---@field CapsLock integer
---@field NumLock integer
---@field ScrollLock integer
---@field Up integer
---@field Down integer
---@field Left integer
---@field Right integer
---@field PageUp integer
---@field PageDown integer
---@field Home integer
---@field End integer
---@field Insert integer
---@field Delete integer
---@field PrintScreen integer
---@field Pause integer
---@field Space integer
---@field Apostrophe integer
---@field Comma integer
---@field Minus integer
---@field Period integer
---@field Slash integer
---@field Semicolon integer
---@field Equal integer
---@field LeftBracket integer
---@field Backslash integer
---@field RightBracket integer
---@field GraveAccent integer
---@field Escape integer
---@field Enter integer
---@field Tab integer
---@field Backspace integer
---@field Menu integer
---@field VolumeUp integer
---@field VolumeDown integer
---@field VolumeMute integer
---@field MediaNext integer
---@field MediaPrevious integer
---@field MediaStop integer
---@field MediaPlayPause integer
---@field MouseLeft integer
---@field MouseRight integer
---@field MouseMiddle integer
---@field MouseButton4 integer
---@field MouseButton5 integer
---@field MouseButton6 integer
---@field MouseButton7 integer
---@field MouseButton8 integer
---@field MouseWheelUp integer
---@field MouseWheelDown integer
---@field GamepadA integer
---@field GamepadB integer
---@field GamepadX integer
---@field GamepadY integer
---@field GamepadLeftBumper integer
---@field GamepadRightBumper integer
---@field GamepadBack integer
---@field GamepadStart integer
---@field GamepadGuide integer
---@field GamepadLeftStick integer
---@field GamepadRightStick integer
---@field GamepadDPadUp integer
---@field GamepadDPadDown integer
---@field GamepadDPadLeft integer
---@field GamepadDPadRight integer
---@field GamepadLeftTrigger integer
---@field GamepadRightTrigger integer
Key = {}

---@class Log
Log = {}

--- Logs an info message.
function Log.info(message) end
--- Logs a warning message.
function Log.warn(message) end
--- Logs an error message.
function Log.error(message) end

---@class Input
Input = {}

function Input.is_key_down(key) end
function Input.is_mouse_button_down(key) end
function Input.get_mouse_x() end
function Input.get_mouse_y() end
function Input.get_mouse_delta_x() end
function Input.get_mouse_delta_y() end

---@class vec2
---@field x number
---@field y number
vec2 = {}

function vec2.new() return vec2 end
function vec2.new(x) return vec2 end
function vec2.new(x, y) return vec2 end

---@class vec3
---@field x number
---@field y number
---@field z number
vec3 = {}

function vec3.new() return vec3 end
function vec3.new(x) return vec3 end
function vec3.new(x, y, z) return vec3 end

---@class vec4
---@field x number
---@field y number
---@field z number
---@field w number
vec4 = {}

function vec4.new() return vec4 end
function vec4.new(x) return vec4 end
function vec4.new(x, y, z, w) return vec4 end

---@class quat
---@field x number
---@field y number
---@field z number
---@field w number
quat = {}

function quat.new() return quat end
function quat.new(w, x, y, z) return quat end

---@class Rigidbody
Rigidbody = {}

function Rigidbody:SetMass(mass) end
function Rigidbody:SetType(type) end
function Rigidbody:SetLinearVelocity(x, y, z) end
function Rigidbody:GetLinearVelocity() end
function Rigidbody:ApplyForceToCenter(x, y, z) end

---@class Transform
---@field pos vec3
---@field rot quat
---@field scale vec3
Transform = {}


---@class Animator
Animator = {}

function Animator:set_float(value) end
function Animator:set_bool(value) end
function Animator:set_int(value) end

---@class Camera
---@field active boolean
---@field fov number
---@field near_plane number
---@field far_plane number
Camera = {}


---@class Entity
Entity = {}

function Entity:GetUUID() end
function Entity:has_component(component_type) end
function Entity:get_component(component_type) end

