#include "Hydrogen/Scene/ScriptEngine.hpp"
#include "Hydrogen/Scene/Animation.hpp"
#include "Hydrogen/Scene/Components.hpp"
#include "Hydrogen/Input.hpp"

using namespace Hydrogen;

void ScriptSystem::OnCreate()
{
    m_Scene->IterateComponents<ScriptsComponent>([&](Entity entity, ScriptsComponent& scripts)
        {
            for (auto& script : scripts.Scripts)
            {
                if (!script->Script) continue;

                script->Environment = sol::environment(
                    ScriptEngine::GetLuaState(),
                    sol::create,
                    ScriptEngine::GetLuaState().globals()
                );

                script->Environment["entity"] = entity;
                script->Environment["self"] = entity;

                sol::load_result loadResult = ScriptEngine::GetLuaState().load(script->Script->GetContent());

                if (!loadResult.valid())
                {
                    sol::error err = loadResult;
                    HY_ENGINE_ERROR("[LUA LOAD ERROR]: {}", err.what());
                    continue;
                }

                sol::protected_function chunkFunc = loadResult;
                sol::set_environment(script->Environment, chunkFunc);

                sol::protected_function_result result = chunkFunc();
                if (!result.valid())
                {
                    sol::error err = result;
                    HY_ENGINE_ERROR("[LUA RUNTIME ERROR]: {}", err.what());
                    continue;
                }

                sol::protected_function on_create = script->Environment["on_create"];
                if (on_create.valid())
                {
                    sol::protected_function_result r = on_create();
                    if (!r.valid())
                    {
                        sol::error err = r;
                        HY_ENGINE_ERROR("[LUA RUNTIME ERROR]: {}", err.what());
                    }
                }
            }
        });
}

void ScriptSystem::OnUpdate(float dt)
{
	m_Scene->IterateComponents<ScriptsComponent>([&](Entity entity, ScriptsComponent& scripts)
		{
			for (const auto& script : scripts.Scripts)
			{
				if (!script->Script) continue;

				sol::protected_function on_update = script->Environment["on_update"];
				if (on_update.valid())
				{
					sol::protected_function_result r = on_update(dt);
					if (!r.valid())
					{
						sol::error err = r;
						HY_ENGINE_ERROR("[LUA RUNTIME ERROR]: {}", err.what());
					}
				}
			}
		});
}

void ScriptEngine::Init()
{
    s_Lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table, sol::lib::string);

    BindMath();
    BindPhysics();
    BindComponents();
    BindEntity();
    BindInputSystem();
    BindLogging();
}

void ScriptEngine::BindEntity()
{
    auto& lua = s_Lua;

    lua.new_usertype<Entity>("Entity",
        "GetUUID", &Entity::GetUUID,

        "get_component", [](Entity& self, const std::string& typeName, sol::this_state s) -> sol::object {
            sol::state_view luaState(s);

            if (typeName == "Transform" && self.HasComponent<TransformComponent>())
                return sol::make_object(luaState, &self.GetComponent<TransformComponent>());

            if (typeName == "Animator" && self.HasComponent<AnimatorComponent>())
                return sol::make_object(luaState, &self.GetComponent<AnimatorComponent>());

            if (typeName == "Rigidbody" && self.HasComponent<RigidbodyComponent>())
                return sol::make_object(luaState, &self.GetComponent<RigidbodyComponent>());

            return sol::nil;
        },

        "has_component", [](Entity& self, const std::string& typeName) -> bool {
            if (typeName == "Transform") return self.HasComponent<TransformComponent>();
            if (typeName == "Animator") return self.HasComponent<AnimatorComponent>();
            if (typeName == "Rigidbody") return self.HasComponent<RigidbodyComponent>();
            return false;
        }
    );
}

void ScriptEngine::BindComponents()
{
	auto& lua = s_Lua;

	lua.new_usertype<TransformComponent>("Transform",
		"pos", sol::property(&TransformComponent::GetTranslation, &TransformComponent::SetTranslation),
		"rot", sol::property(&TransformComponent::GetRotation, &TransformComponent::SetRotation),
		"scale", sol::property(&TransformComponent::GetScale, &TransformComponent::SetScale)
	);

	lua.new_usertype<AnimatorComponent>("Animator",
		"set_float", &AnimatorComponent::SetFloat,
		"set_bool", &AnimatorComponent::SetBool,
		"set_int", &AnimatorComponent::SetInt
	);

	lua["Transform"] = "Transform";
	lua["Animator"] = "Animator";
	lua["Rigidbody"] = "Rigidbody";
}

void ScriptEngine::BindMath()
{
	auto& lua = s_Lua;

	lua.new_usertype<glm::vec3>("vec3",
		sol::constructors<glm::vec3(), glm::vec3(float), glm::vec3(float, float, float)>(),
		"x", &glm::vec3::x,
		"y", &glm::vec3::y,
		"z", &glm::vec3::z,

		sol::meta_function::addition, [](const glm::vec3& a, const glm::vec3& b) { return a + b; },
		sol::meta_function::subtraction, [](const glm::vec3& a, const glm::vec3& b) { return a - b; },
		sol::meta_function::multiplication, sol::overload(
			[](const glm::vec3& v, float scalar) { return v * scalar; },
			[](float scalar, const glm::vec3& v) { return v * scalar; },
			[](const glm::vec3& a, const glm::vec3& b) { return a * b; }
		)
	);
}

void ScriptEngine::BindInputSystem()
{
	auto& lua = s_Lua;

	lua.new_enum<KeyCode>("Key", {
		{ "Unknown", KeyCode::Unknown },

		{ "A", KeyCode::A }, { "B", KeyCode::B }, { "C", KeyCode::C }, { "D", KeyCode::D },
		{ "E", KeyCode::E }, { "F", KeyCode::F }, { "G", KeyCode::G }, { "H", KeyCode::H },
		{ "I", KeyCode::I }, { "J", KeyCode::J }, { "K", KeyCode::K }, { "L", KeyCode::L },
		{ "M", KeyCode::M }, { "N", KeyCode::N }, { "O", KeyCode::O }, { "P", KeyCode::P },
		{ "Q", KeyCode::Q }, { "R", KeyCode::R }, { "S", KeyCode::S }, { "T", KeyCode::T },
		{ "U", KeyCode::U }, { "V", KeyCode::V }, { "W", KeyCode::W }, { "X", KeyCode::X },
		{ "Y", KeyCode::Y }, { "Z", KeyCode::Z },

		{ "Num0", KeyCode::Num0 }, { "Num1", KeyCode::Num1 }, { "Num2", KeyCode::Num2 },
		{ "Num3", KeyCode::Num3 }, { "Num4", KeyCode::Num4 }, { "Num5", KeyCode::Num5 },
		{ "Num6", KeyCode::Num6 }, { "Num7", KeyCode::Num7 }, { "Num8", KeyCode::Num8 },
		{ "Num9", KeyCode::Num9 },

		{ "F1", KeyCode::F1 },   { "F2", KeyCode::F2 },   { "F3", KeyCode::F3 },
		{ "F4", KeyCode::F4 },   { "F5", KeyCode::F5 },   { "F6", KeyCode::F6 },
		{ "F7", KeyCode::F7 },   { "F8", KeyCode::F8 },   { "F9", KeyCode::F9 },
		{ "F10", KeyCode::F10 }, { "F11", KeyCode::F11 }, { "F12", KeyCode::F12 },

		{ "LeftShift", KeyCode::LeftShift },     { "RightShift", KeyCode::RightShift },
		{ "LeftControl", KeyCode::LeftControl }, { "RightControl", KeyCode::RightControl },
		{ "LeftAlt", KeyCode::LeftAlt },         { "RightAlt", KeyCode::RightAlt },
		{ "Space", KeyCode::Space },             { "Escape", KeyCode::Escape },
		{ "Enter", KeyCode::Enter },             { "Tab", KeyCode::Tab },
		{ "Backspace", KeyCode::Backspace },

		{ "Up", KeyCode::Up },       { "Down", KeyCode::Down },
		{ "Left", KeyCode::Left },   { "Right", KeyCode::Right },

		{ "MouseLeft", KeyCode::MouseLeft },     { "MouseRight", KeyCode::MouseRight },
		{ "MouseMiddle", KeyCode::MouseMiddle }, { "MouseButton4", KeyCode::MouseButton4 },
		{ "MouseButton5", KeyCode::MouseButton5 },

		{ "GamepadA", KeyCode::GamepadA },           { "GamepadB", KeyCode::GamepadB },
		{ "GamepadX", KeyCode::GamepadX },           { "GamepadY", KeyCode::GamepadY },
		{ "GamepadLeftBumper", KeyCode::GamepadLeftBumper },
		{ "GamepadRightBumper", KeyCode::GamepadRightBumper },
		{ "GamepadDPadUp", KeyCode::GamepadDPadUp },
		{ "GamepadDPadDown", KeyCode::GamepadDPadDown },
		{ "GamepadDPadLeft", KeyCode::GamepadDPadLeft },
		{ "GamepadDPadRight", KeyCode::GamepadDPadRight }
		});

	auto inputTable = lua.create_named_table("Input");

	inputTable.set_function("is_key_down", &Input::IsKeyDown);
	inputTable.set_function("is_mouse_button_down", &Input::IsMouseButtonDown);

	inputTable.set_function("get_mouse_x", &Input::GetMouseX);
	inputTable.set_function("get_mouse_y", &Input::GetMouseY);
	inputTable.set_function("get_mouse_delta_x", &Input::GetMouseDeltaX);
	inputTable.set_function("get_mouse_delta_y", &Input::GetMouseDeltaY);
}

void ScriptEngine::BindPhysics()
{
	auto& lua = s_Lua;

	lua.new_enum<reactphysics3d::BodyType>("BodyType",
		{
			{ "STATIC", reactphysics3d::BodyType::STATIC },
			{ "KINEMATIC", reactphysics3d::BodyType::KINEMATIC },
			{ "DYNAMIC", reactphysics3d::BodyType::DYNAMIC }
		}
	);

	lua.new_usertype<RigidbodyComponent>("Rigidbody",
		"SetLinearVelocity", [](RigidbodyComponent& rb, float x, float y, float z) {
			if (rb.Rigidbody)
			{
				rb.Rigidbody->setLinearVelocity(reactphysics3d::Vector3(x, y, z));
			}
		},
		"GetLinearVelocity", [](RigidbodyComponent& rb) {
			if (rb.Rigidbody)
			{
				reactphysics3d::Vector3 v = rb.Rigidbody->getLinearVelocity();
				return std::make_tuple(v.x, v.y, v.z);
			}
			return std::make_tuple(0.0f, 0.0f, 0.0f);
		},
		"ApplyForceToCenter", [](RigidbodyComponent& rb, float x, float y, float z) {
			if (rb.Rigidbody)
			{
				rb.Rigidbody->applyWorldForceAtCenterOfMass(reactphysics3d::Vector3(x, y, z));
			}
		},
		"SetMass", &RigidbodyComponent::SetMass,
		"SetType", &RigidbodyComponent::SetType
	);
}

void ScriptEngine::BindLogging()
{
	auto& lua = s_Lua;

	auto logTable = lua.create_named_table("Log");

	logTable.set_function("info", [](const std::string& message) {
		HY_APP_INFO("Script: {}", message);
		});

	logTable.set_function("warn", [](const std::string& message) {
		HY_APP_WARN("Script: {}", message);
		});

	logTable.set_function("error", [](const std::string& message) {
		HY_APP_ERROR("Script: {}", message);
		});
}
