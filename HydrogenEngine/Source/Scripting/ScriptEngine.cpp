#include "Hydrogen/Scripting/ScriptEngine.hpp"
#include "Hydrogen/Scene/Animation.hpp"
#include "Hydrogen/Scene/Components.hpp"
#include "Hydrogen/Input.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Application.hpp"

using namespace Hydrogen;

static SolScriptBackend s_SolBackend;

class LuaScriptInstance : public ScriptInstance
{
public:
	LuaScriptInstance(std::shared_ptr<ScriptAsset> asset, Entity entity)
		: m_Asset(asset), m_Entity(entity)
	{
		if (!m_Asset)
			return;

		auto& lua = s_SolBackend.GetState();

		std::string scriptSource = m_Asset->GetContent();

		sol::load_result loadedScript = lua.load(scriptSource);
		if (!loadedScript.valid())
		{
			sol::error err = loadedScript;
			HY_ENGINE_ERROR("[LUA LOAD ERROR]: {}", err.what());
			return;
		}

		sol::protected_function_result result = loadedScript();
		if (!result.valid())
		{
			sol::error err = result;
			return;
		}

		m_ScriptTable = result;
		m_ScriptTable["entity"] = m_Entity;
	}

	virtual void OnCreate() override
	{
		if (!m_ScriptTable.valid())
			return;

		sol::protected_function onCreate = m_ScriptTable["on_create"];
		if (onCreate.valid())
		{
			sol::protected_function_result result = onCreate(m_ScriptTable);
			if (!result.valid())
			{
				sol::error err = result;
				HY_ENGINE_ERROR("[LUA RUNTIME ERROR]: {}", err.what());
			}
		}
	}

	virtual void OnUpdate(float dt) override
	{
		if (!m_ScriptTable.valid())
			return;

		sol::protected_function onUpdate = m_ScriptTable["on_update"];
		if (onUpdate.valid())
		{
			sol::protected_function_result result = onUpdate(m_ScriptTable, dt);
			if (!result.valid())
			{
				sol::error err = result;
				HY_ENGINE_ERROR("[LUA RUNTIME ERROR]: {}", err.what());
			}
		}
	}

	virtual void ParseMetadata(std::vector<ScriptFieldMetadata>& outFields) override
	{
		UpdateMetadata(outFields); // serialized fields set their initial values
		outFields.clear();

		if (!m_ScriptTable.valid())
			return;

		sol::table fieldsTable = m_ScriptTable["properties"];
		if (!fieldsTable.valid())
			return;

		fieldsTable.for_each([this, &outFields](sol::object key, sol::object value) {
			if (key.is<std::string>() && value.is<sol::table>())
			{
				std::string fieldName = key.as<std::string>();
				sol::table fieldDef = value.as<sol::table>();

				ScriptFieldMetadata metadata;
				metadata.Name = fieldName;
				metadata.Type = ScriptFieldType::Unknown;

				std::string typeName = fieldDef["type"].get_or<std::string>("float");
				if (typeName == "float")
				{
					metadata.Type = ScriptFieldType::Float;
					metadata.Value = fieldDef["value"].get_or<double>(0.0f);
				}
				if (typeName == "int")
				{
					metadata.Type = ScriptFieldType::Int;
					metadata.Value = fieldDef["value"].get_or<int64_t>(0);
				}
				if (typeName == "bool")
				{
					metadata.Type = ScriptFieldType::Bool;
					metadata.Value = fieldDef["value"].get_or(false);
				}
				if (typeName == "string")
				{
					metadata.Type = ScriptFieldType::String;
					metadata.Value = fieldDef["value"].get_or<std::string>("");
				}

				if (typeName == "entity")
				{
					metadata.Type = ScriptFieldType::Entity;

					sol::object valObj = fieldDef["value"];
					if (valObj.is<Entity>())
					{
						metadata.Value = valObj.as<Entity>();
					}
					else
					{
						metadata.Value = Entity();
					}
				}

				outFields.push_back(metadata);
			}
			});
	}

	virtual void UpdateMetadata(const std::vector<ScriptFieldMetadata>& fields) override
	{
		if (!m_ScriptTable.valid())
			return;

		sol::table fieldsTable = m_ScriptTable["properties"];
		if (!fieldsTable.valid())
			return;

		for (const auto& field : fields)
		{
			if (fieldsTable[field.Name] == sol::nil)
				continue;

			sol::table fieldDef = fieldsTable[field.Name];
			if (!fieldDef.valid())
				continue;

			std::visit([&fieldDef](auto&& arg) {
				fieldDef["value"] = arg;
				}, field.Value);
		}
	}

private:
	std::shared_ptr<ScriptAsset> m_Asset;
	Entity m_Entity;

	sol::table m_ScriptTable;
};

std::unique_ptr<ScriptInstance> Hydrogen::CreateLuaScriptInstance(std::shared_ptr<ScriptAsset> asset, Entity entity)
{
	return std::make_unique<LuaScriptInstance>(asset, entity);
}

void ScriptSystem::IndexScripts()
{
	HY_ENGINE_INFO("Reloading scripting");

	m_Scene->IterateComponents<ScriptsComponent>([](Entity entity, ScriptsComponent& scripts)
		{
			for (const auto& scriptDesc : scripts.Scripts)
			{
				if (!scriptDesc->Script) return;
				if (scriptDesc->Instance) scriptDesc->Instance.reset();


				scriptDesc->Instance = CreateLuaScriptInstance(scriptDesc->Script, entity);
				scriptDesc->Instance->ParseMetadata(scriptDesc->ExposedFields);
			}
		});
}

void ScriptSystem::OnInit()
{
	m_Scene->IterateComponents<ScriptsComponent>([](Entity entity, ScriptsComponent& scripts)
		{
			for (const auto& scriptDesc : scripts.Scripts)
			{
				if (!scriptDesc->Script) return;

				if (!scriptDesc->Instance)
				{
					scriptDesc->Instance = CreateLuaScriptInstance(scriptDesc->Script, entity);
					scriptDesc->Instance->ParseMetadata(scriptDesc->ExposedFields);
				}

				scriptDesc->Instance->OnCreate();
			}
		});
}

void ScriptSystem::OnUpdate(float dt)
{
	m_Scene->IterateComponents<ScriptsComponent>([dt](Entity entity, ScriptsComponent& scripts)
		{
			for (const auto& script : scripts.Scripts)
			{
				if (!script->Script) return;

				if (!script->Instance)
				{
					script->Instance = CreateLuaScriptInstance(script->Script, entity);
					script->Instance->ParseMetadata(script->ExposedFields);
					script->Instance->OnCreate();
				}

				if (script->ExposedFieldsDirty)
				{
					script->Instance->UpdateMetadata(script->ExposedFields);
					script->ExposedFieldsDirty = false;
				}
				script->Instance->OnUpdate(dt);
			}
		});
}

class LoggingScriptModule : public ScriptModule
{
public:
	void RegisterBindings(ScriptRegistry& registry) override
	{
		auto logNs = registry.BeginNamespace("Log");

		logNs.Function("info", [](const std::string& message) {
			HY_APP_INFO("Script: {}", message);
			}, "(message)", "Logs an info message.");

		logNs.Function("warn", [](const std::string& message) {
			HY_APP_WARN("Script: {}", message);
			}, "(message)", "Logs a warning message.");

		logNs.Function("error", [](const std::string& message) {
			HY_APP_ERROR("Script: {}", message);
			}, "(message)", "Logs an error message.");
	}
};

class MathScriptModule : public ScriptModule
{
public:
	void RegisterBindings(ScriptRegistry& registry) override
	{
		registry.BeginClass<glm::vec2>("vec2")
			.Constructor<glm::vec2(void)>("()")
			.Constructor<glm::vec2(float)>("(x)")
			.Constructor<glm::vec2(float, float)>("(x, y)")
			.Property("x", "number", [](const glm::vec2& v) { return v.x; }, [](glm::vec2& v, float x) { v.x = x; })
			.Property("y", "number", [](const glm::vec2& v) { return v.y; }, [](glm::vec2& v, float y) { v.y = y; })
			.Operator(ScriptOperator::Add, [](const glm::vec2& a, const glm::vec2& b) { return a + b; })
			.Operator(ScriptOperator::Subtract, [](const glm::vec2& a, const glm::vec2& b) { return a - b; })
			.Operator(ScriptOperator::Multiply, [](const glm::vec2& v, float scalar) { return v * scalar; })
			.Operator(ScriptOperator::Multiply, [](float scalar, const glm::vec2& v) { return v * scalar; });

		registry.BeginClass<glm::vec3>("vec3")
			.Constructor<glm::vec3(void)>("()")
			.Constructor<glm::vec3(float)>("(x)")
			.Constructor<glm::vec3(float, float, float)>("(x, y, z)")
			.Property("x", "number", [](const glm::vec3& v) { return v.x; }, [](glm::vec3& v, float x) { v.x = x; })
			.Property("y", "number", [](const glm::vec3& v) { return v.y; }, [](glm::vec3& v, float y) { v.y = y; })
			.Property("z", "number", [](const glm::vec3& v) { return v.z; }, [](glm::vec3& v, float z) { v.z = z; })

			.Operator(ScriptOperator::Add, [](const glm::vec3& a, const glm::vec3& b) { return a + b; })
			.Operator(ScriptOperator::Subtract, [](const glm::vec3& a, const glm::vec3& b) { return a - b; })

			.Operator(ScriptOperator::Multiply, [](const glm::vec3& a, const glm::vec3& b) { return a * b; })
			.Operator(ScriptOperator::Multiply, [](const glm::vec3& v, float scalar) { return v * scalar; })
			.Operator(ScriptOperator::Multiply, [](float scalar, const glm::vec3& v) { return v * scalar; });

		registry.BeginClass<glm::vec4>("vec4")
			.Constructor<glm::vec4(void)>("()")
			.Constructor<glm::vec4(float)>("(x)")
			.Constructor<glm::vec4(float, float, float, float)>("(x, y, z, w)")
			.Property("x", "number", [](const glm::vec4& v) { return v.x; }, [](glm::vec4& v, float x) { v.x = x; })
			.Property("y", "number", [](const glm::vec4& v) { return v.y; }, [](glm::vec4& v, float y) { v.y = y; })
			.Property("z", "number", [](const glm::vec4& v) { return v.z; }, [](glm::vec4& v, float z) { v.z = z; })
			.Property("w", "number", [](const glm::vec4& v) { return v.w; }, [](glm::vec4& v, float w) { v.w = w; })
			.Operator(ScriptOperator::Add, [](const glm::vec4& a, const glm::vec4& b) { return a + b; })
			.Operator(ScriptOperator::Subtract, [](const glm::vec4& a, const glm::vec4& b) { return a - b; })
			.Operator(ScriptOperator::Multiply, [](const glm::vec4& v, float scalar) { return v * scalar; })
			.Operator(ScriptOperator::Multiply, [](float scalar, const glm::vec4& v) { return v * scalar; });

		registry.BeginClass<glm::quat>("quat")
			.Constructor<glm::quat()>("()")
			.Constructor<glm::quat(float, float, float, float)>("(w, x, y, z)")
			.Property("x", "number", [](const glm::quat& q) { return q.x; }, [](glm::quat& q, float x) { q.x = x; })
			.Property("y", "number", [](const glm::quat& q) { return q.y; }, [](glm::quat& q, float y) { q.y = y; })
			.Property("z", "number", [](const glm::quat& q) { return q.z; }, [](glm::quat& q, float z) { q.z = z; })
			.Property("w", "number", [](const glm::quat& q) { return q.w; }, [](glm::quat& q, float w) { q.w = w; })
			.Operator(ScriptOperator::Multiply, [](const glm::quat& a, const glm::quat& b) { return a * b; })
			.Operator(ScriptOperator::Multiply, [](const glm::quat& q, const glm::vec3& v) { return q * v; });
	}
};

class PhysicsScriptModule : public ScriptModule
{
public:
	void RegisterBindings(ScriptRegistry& registry) override
	{
		registry.BeginEnum<reactphysics3d::BodyType>("BodyType")
			.Value("STATIC", reactphysics3d::BodyType::STATIC)
			.Value("KINEMATIC", reactphysics3d::BodyType::KINEMATIC)
			.Value("DYNAMIC", reactphysics3d::BodyType::DYNAMIC);

		registry.BeginClass<RigidbodyComponent>("Rigidbody")
			.Method("SetMass", &RigidbodyComponent::SetMass, "(mass)")
			.Method("SetType", &RigidbodyComponent::SetType, "(type)")

			.Method("SetLinearVelocity", [](RigidbodyComponent& rb, float x, float y, float z) {
			if (rb.Rigidbody)
				rb.Rigidbody->setLinearVelocity(reactphysics3d::Vector3(x, y, z));
				}, "(x, y, z)")

			.Method("GetLinearVelocity", [](RigidbodyComponent& rb) {
			if (rb.Rigidbody) {
				auto v = rb.Rigidbody->getLinearVelocity();
				return std::make_tuple(v.x, v.y, v.z);
			}
			return std::make_tuple(0.0f, 0.0f, 0.0f);
				}, "()")

			.Method("ApplyForceToCenter", [](RigidbodyComponent& rb, float x, float y, float z) {
			if (rb.Rigidbody)
				rb.Rigidbody->applyWorldForceAtCenterOfMass(reactphysics3d::Vector3(x, y, z));
				}, "(x, y, z)");
	}
};

class InputScriptModule : public ScriptModule
{
public:
	void RegisterBindings(ScriptRegistry& registry) override
	{
		registry.BeginEnum<KeyCode>("Key")
			.Value("Unknown", KeyCode::Unknown)

			// Letters
			.Value("A", KeyCode::A)
			.Value("B", KeyCode::B)
			.Value("C", KeyCode::C)
			.Value("D", KeyCode::D)
			.Value("E", KeyCode::E)
			.Value("F", KeyCode::F)
			.Value("G", KeyCode::G)
			.Value("H", KeyCode::H)
			.Value("I", KeyCode::I)
			.Value("J", KeyCode::J)
			.Value("K", KeyCode::K)
			.Value("L", KeyCode::L)
			.Value("M", KeyCode::M)
			.Value("N", KeyCode::N)
			.Value("O", KeyCode::O)
			.Value("P", KeyCode::P)
			.Value("Q", KeyCode::Q)
			.Value("R", KeyCode::R)
			.Value("S", KeyCode::S)
			.Value("T", KeyCode::T)
			.Value("U", KeyCode::U)
			.Value("V", KeyCode::V)
			.Value("W", KeyCode::W)
			.Value("X", KeyCode::X)
			.Value("Y", KeyCode::Y)
			.Value("Z", KeyCode::Z)

			// Numbers
			.Value("Num0", KeyCode::Num0)
			.Value("Num1", KeyCode::Num1)
			.Value("Num2", KeyCode::Num2)
			.Value("Num3", KeyCode::Num3)
			.Value("Num4", KeyCode::Num4)
			.Value("Num5", KeyCode::Num5)
			.Value("Num6", KeyCode::Num6)
			.Value("Num7", KeyCode::Num7)
			.Value("Num8", KeyCode::Num8)
			.Value("Num9", KeyCode::Num9)

			// Function Keys
			.Value("F1", KeyCode::F1)
			.Value("F2", KeyCode::F2)
			.Value("F3", KeyCode::F3)
			.Value("F4", KeyCode::F4)
			.Value("F5", KeyCode::F5)
			.Value("F6", KeyCode::F6)
			.Value("F7", KeyCode::F7)
			.Value("F8", KeyCode::F8)
			.Value("F9", KeyCode::F9)
			.Value("F10", KeyCode::F10)
			.Value("F11", KeyCode::F11)
			.Value("F12", KeyCode::F12)
			.Value("F13", KeyCode::F13)
			.Value("F14", KeyCode::F14)
			.Value("F15", KeyCode::F15)
			.Value("F16", KeyCode::F16)
			.Value("F17", KeyCode::F17)
			.Value("F18", KeyCode::F18)
			.Value("F19", KeyCode::F19)
			.Value("F20", KeyCode::F20)
			.Value("F21", KeyCode::F21)
			.Value("F22", KeyCode::F22)
			.Value("F23", KeyCode::F23)
			.Value("F24", KeyCode::F24)

			// Keypad
			.Value("KP_0", KeyCode::KP_0)
			.Value("KP_1", KeyCode::KP_1)
			.Value("KP_2", KeyCode::KP_2)
			.Value("KP_3", KeyCode::KP_3)
			.Value("KP_4", KeyCode::KP_4)
			.Value("KP_5", KeyCode::KP_5)
			.Value("KP_6", KeyCode::KP_6)
			.Value("KP_7", KeyCode::KP_7)
			.Value("KP_8", KeyCode::KP_8)
			.Value("KP_9", KeyCode::KP_9)
			.Value("KP_Decimal", KeyCode::KP_Decimal)
			.Value("KP_Divide", KeyCode::KP_Divide)
			.Value("KP_Multiply", KeyCode::KP_Multiply)
			.Value("KP_Subtract", KeyCode::KP_Subtract)
			.Value("KP_Add", KeyCode::KP_Add)
			.Value("KP_Enter", KeyCode::KP_Enter)
			.Value("KP_Equal", KeyCode::KP_Equal)

			// Modifiers & Locks
			.Value("LeftShift", KeyCode::LeftShift)
			.Value("RightShift", KeyCode::RightShift)
			.Value("LeftControl", KeyCode::LeftControl)
			.Value("RightControl", KeyCode::RightControl)
			.Value("LeftAlt", KeyCode::LeftAlt)
			.Value("RightAlt", KeyCode::RightAlt)
			.Value("LeftSuper", KeyCode::LeftSuper)
			.Value("RightSuper", KeyCode::RightSuper)
			.Value("CapsLock", KeyCode::CapsLock)
			.Value("NumLock", KeyCode::NumLock)
			.Value("ScrollLock", KeyCode::ScrollLock)

			// Navigation & Editing
			.Value("Up", KeyCode::Up)
			.Value("Down", KeyCode::Down)
			.Value("Left", KeyCode::Left)
			.Value("Right", KeyCode::Right)
			.Value("PageUp", KeyCode::PageUp)
			.Value("PageDown", KeyCode::PageDown)
			.Value("Home", KeyCode::Home)
			.Value("End", KeyCode::End)
			.Value("Insert", KeyCode::Insert)
			.Value("Delete", KeyCode::Delete)
			.Value("PrintScreen", KeyCode::PrintScreen)
			.Value("Pause", KeyCode::Pause)

			// Punctuation & Symbols
			.Value("Space", KeyCode::Space)
			.Value("Apostrophe", KeyCode::Apostrophe)
			.Value("Comma", KeyCode::Comma)
			.Value("Minus", KeyCode::Minus)
			.Value("Period", KeyCode::Period)
			.Value("Slash", KeyCode::Slash)
			.Value("Semicolon", KeyCode::Semicolon)
			.Value("Equal", KeyCode::Equal)
			.Value("LeftBracket", KeyCode::LeftBracket)
			.Value("Backslash", KeyCode::Backslash)
			.Value("RightBracket", KeyCode::RightBracket)
			.Value("GraveAccent", KeyCode::GraveAccent)

			// Controls & System
			.Value("Escape", KeyCode::Escape)
			.Value("Enter", KeyCode::Enter)
			.Value("Tab", KeyCode::Tab)
			.Value("Backspace", KeyCode::Backspace)
			.Value("Menu", KeyCode::Menu)

			// Media Controls
			.Value("VolumeUp", KeyCode::VolumeUp)
			.Value("VolumeDown", KeyCode::VolumeDown)
			.Value("VolumeMute", KeyCode::VolumeMute)
			.Value("MediaNext", KeyCode::MediaNext)
			.Value("MediaPrevious", KeyCode::MediaPrevious)
			.Value("MediaStop", KeyCode::MediaStop)
			.Value("MediaPlayPause", KeyCode::MediaPlayPause)

			// Mouse Buttons
			.Value("MouseLeft", KeyCode::MouseLeft)
			.Value("MouseRight", KeyCode::MouseRight)
			.Value("MouseMiddle", KeyCode::MouseMiddle)
			.Value("MouseButton4", KeyCode::MouseButton4)
			.Value("MouseButton5", KeyCode::MouseButton5)
			.Value("MouseButton6", KeyCode::MouseButton6)
			.Value("MouseButton7", KeyCode::MouseButton7)
			.Value("MouseButton8", KeyCode::MouseButton8)
			.Value("MouseWheelUp", KeyCode::MouseWheelUp)
			.Value("MouseWheelDown", KeyCode::MouseWheelDown)

			// Gamepad
			.Value("GamepadA", KeyCode::GamepadA)
			.Value("GamepadB", KeyCode::GamepadB)
			.Value("GamepadX", KeyCode::GamepadX)
			.Value("GamepadY", KeyCode::GamepadY)
			.Value("GamepadLeftBumper", KeyCode::GamepadLeftBumper)
			.Value("GamepadRightBumper", KeyCode::GamepadRightBumper)
			.Value("GamepadBack", KeyCode::GamepadBack)
			.Value("GamepadStart", KeyCode::GamepadStart)
			.Value("GamepadGuide", KeyCode::GamepadGuide)
			.Value("GamepadLeftStick", KeyCode::GamepadLeftStick)
			.Value("GamepadRightStick", KeyCode::GamepadRightStick)
			.Value("GamepadDPadUp", KeyCode::GamepadDPadUp)
			.Value("GamepadDPadDown", KeyCode::GamepadDPadDown)
			.Value("GamepadDPadLeft", KeyCode::GamepadDPadLeft)
			.Value("GamepadDPadRight", KeyCode::GamepadDPadRight)
			.Value("GamepadLeftTrigger", KeyCode::GamepadLeftTrigger)
			.Value("GamepadRightTrigger", KeyCode::GamepadRightTrigger);

		auto inputNs = registry.BeginNamespace("Input");
		inputNs.Function("is_key_down", &Input::IsKeyDown, "(key)");
		inputNs.Function("is_mouse_button_down", &Input::IsMouseButtonDown, "(key)");

		inputNs.Function("get_mouse_x", &Input::GetMouseX, "()");
		inputNs.Function("get_mouse_y", &Input::GetMouseY, "()");
		inputNs.Function("get_mouse_delta_x", &Input::GetMouseDeltaX, "()");
		inputNs.Function("get_mouse_delta_y", &Input::GetMouseDeltaY, "()");
	}
};

class EntityScriptModule : public ScriptModule
{
public:
	void RegisterBindings(ScriptRegistry& registry) override
	{
		registry.BeginClass<TransformComponent>("Transform")
			.Property("pos", "vec3", &TransformComponent::GetTranslation, &TransformComponent::SetTranslation)
			.Property("rot", "quat", &TransformComponent::GetRotation, &TransformComponent::SetRotation)
			.Property("scale", "vec3", &TransformComponent::GetScale, &TransformComponent::SetScale);

		registry.BeginClass<AnimatorComponent>("Animator")
			.Method("set_float", &AnimatorComponent::SetFloat, "(value)")
			.Method("set_bool", &AnimatorComponent::SetBool, "(value)")
			.Method("set_int", &AnimatorComponent::SetInt, "(value)");

		registry.BeginClass<CameraComponent>("Camera")
			.Property("active", "boolean", &CameraComponent::GetActive, &CameraComponent::SetActive)
			.Property("fov", "number", &CameraComponent::GetFOV, &CameraComponent::SetFOV)
			.Property("near_plane", "number", &CameraComponent::GetNearPlane, &CameraComponent::SetNearPlane)
			.Property("far_plane", "number", &CameraComponent::GetFarPlane, &CameraComponent::SetFarPlane);

		registry.BeginClass<Entity>("Entity")
			.Method("GetUUID", &Entity::GetUUID, "()")

			.Method("has_component", [](Entity& entity, const std::string& typeName) {
			if (typeName == "Rigidbody")
				return entity.HasComponent<RigidbodyComponent>();
			if (typeName == "Transform")
				return entity.HasComponent<TransformComponent>();
			if (typeName == "Animator")
				return entity.HasComponent<AnimatorComponent>();
			if (typeName == "Camera")
				return entity.HasComponent<CameraComponent>();
			return false;
				}, "(component_type)")

			.Method("get_component", [](Entity& entity, const std::string& typeName, sol::this_state s) -> sol::object {
			sol::state_view lua(s);
			if (typeName == "Rigidbody" && entity.HasComponent<RigidbodyComponent>())
				return sol::make_object(lua, std::ref(entity.GetComponent<RigidbodyComponent>()));
			if (typeName == "Transform" && entity.HasComponent<TransformComponent>())
				return sol::make_object(lua, std::ref(entity.GetComponent<TransformComponent>()));
			if (typeName == "Animator" && entity.HasComponent<AnimatorComponent>())
				return sol::make_object(lua, std::ref(entity.GetComponent<AnimatorComponent>()));
			if (typeName == "Camera" && entity.HasComponent<CameraComponent>())
				return sol::make_object(lua, std::ref(entity.GetComponent<CameraComponent>()));

			return sol::nil;
			}, "(component_type)");
	}
};

void ScriptEngine::Init()
{
	ScriptRegistry registry;

	std::vector<std::unique_ptr<ScriptModule>> modules;
	modules.push_back(std::make_unique<LoggingScriptModule>());
	modules.push_back(std::make_unique<MathScriptModule>());
	modules.push_back(std::make_unique<PhysicsScriptModule>());
	modules.push_back(std::make_unique<InputScriptModule>());
	modules.push_back(std::make_unique<EntityScriptModule>());

	for (auto& mod : modules)
	{
		mod->RegisterBindings(registry);
	}

	s_SolBackend.Build(registry.Database());
	StubBackend("HydrogenEngine.lua").Build(registry.Database());
}
