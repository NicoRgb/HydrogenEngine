#include "Hydrogen/Scripting/ScriptEngine.hpp"
#include "Hydrogen/Scene/Animation.hpp"
#include "Hydrogen/Scene/Components.hpp"
#include "Hydrogen/Input.hpp"
#include "Hydrogen/AssetManager.hpp"

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

		sol::protected_function onUpdate = m_ScriptTable["OnUpdate"];
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

	virtual void ParseMetadata(std::unordered_map<std::string, ScriptFieldMetadata>& outFields) override
	{
		if (!m_ScriptTable.valid())
			return;

		sol::table fieldsTable = m_ScriptTable["properties"];
		if (!fieldsTable.valid())
			return;

		fieldsTable.for_each([&outFields](sol::object key, sol::object value) {
			if (key.is<std::string>() && value.is<sol::table>())
			{
				std::string fieldName = key.as<std::string>();
				sol::table fieldDef = value.as<sol::table>();

				ScriptFieldMetadata metadata;
				metadata.Name = fieldName;
				metadata.Type = ScriptFieldType::Float; // default

				std::string typeName = fieldDef["type"].get_or<std::string>("float");
				if (typeName == "int")
					metadata.Type = ScriptFieldType::Int;
				if (typeName == "bool")
					metadata.Type = ScriptFieldType::Bool;
				if (typeName == "string")
					metadata.Type = ScriptFieldType::String;

				outFields[fieldName] = metadata;
			}
			});
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

void ScriptSystem::OnUpdate(float dt)
{
	m_Scene->IterateComponents<ScriptsComponent>([dt](Entity entity, ScriptsComponent& scripts)
		{
			for (const auto& scriptDesc : scripts.Scripts)
			{
				if (!scriptDesc->Script) return;

				if (!scriptDesc->Instance)
				{
					scriptDesc->Instance = CreateLuaScriptInstance(scriptDesc->Script, entity);
					scriptDesc->Instance->OnCreate();
				}

				scriptDesc->Instance->OnUpdate(dt);
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
			}, "(string)", "Logs an info message.");

		logNs.Function("warn", [](const std::string& message) {
			HY_APP_WARN("Script: {}", message);
			}, "(string)", "Logs a warning message.");

		logNs.Function("error", [](const std::string& message) {
			HY_APP_ERROR("Script: {}", message);
			}, "(string)", "Logs an error message.");
	}
};

class MathScriptModule : public ScriptModule
{
public:
	void RegisterBindings(ScriptRegistry& registry) override
	{
		registry.BeginClass<glm::vec3>("vec3")
			.Constructor<glm::vec3(void)>("()")
			.Constructor<glm::vec3(float)>("(number)")
			.Constructor<glm::vec3(float, float, float)>("(number, number, number)")
			.Property("x", "number", [](const glm::vec3& v) { return v.x; }, [](glm::vec3& v, float x) { v.x = x; })
			.Property("y", "number", [](const glm::vec3& v) { return v.y; }, [](glm::vec3& v, float y) { v.y = y; })
			.Property("z", "number", [](const glm::vec3& v) { return v.z; }, [](glm::vec3& v, float z) { v.z = z; })

			.Operator(ScriptOperator::Add, [](const glm::vec3& a, const glm::vec3& b) { return a + b; })
			.Operator(ScriptOperator::Subtract, [](const glm::vec3& a, const glm::vec3& b) { return a - b; })

			.Operator(ScriptOperator::Multiply, [](const glm::vec3& a, const glm::vec3& b) { return a * b; })
			.Operator(ScriptOperator::Multiply, [](const glm::vec3& v, float scalar) { return v * scalar; })
			.Operator(ScriptOperator::Multiply, [](float scalar, const glm::vec3& v) { return v * scalar; });
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
			.Method("SetMass", &RigidbodyComponent::SetMass, "(number)")
			.Method("SetType", &RigidbodyComponent::SetType, "(number)")

			.Method("SetLinearVelocity", [](RigidbodyComponent& rb, float x, float y, float z) {
			if (rb.Rigidbody)
				rb.Rigidbody->setLinearVelocity(reactphysics3d::Vector3(x, y, z));
				}, "(number, number, number)")

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
				}, "(number, number, number)");
	}
};

class InputScriptModule : public ScriptModule
{
public:
	void RegisterBindings(ScriptRegistry& registry) override
	{
		registry.BeginEnum<KeyCode>("Key")
			.Value("Unknown", KeyCode::Unknown)
			.Value("A", KeyCode::A)
			.Value("B", KeyCode::B)
			// ...
			.Value("Space", KeyCode::Space)
			.Value("Escape", KeyCode::Escape);

		auto inputNs = registry.BeginNamespace("Input");
		inputNs.Function("is_key_down", &Input::IsKeyDown, "()");
		inputNs.Function("is_mouse_button_down", &Input::IsMouseButtonDown, "()");

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
			.Method("set_float", &AnimatorComponent::SetFloat, "(number)")
			.Method("set_bool", &AnimatorComponent::SetBool, "(boolean)")
			.Method("set_int", &AnimatorComponent::SetInt, "(number)");

		registry.BeginClass<Entity>("Entity")
			.Method("GetUUID", &Entity::GetUUID, "()");

		//registry.RegisterComponentType<TransformComponent>("Transform");
		//registry.RegisterComponentType<AnimatorComponent>("Animator");
		//registry.RegisterComponentType<RigidbodyComponent>("Rigidbody");
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
	//StubBackend("HydrogenEngine.stub").Build(registry.Database());
}
