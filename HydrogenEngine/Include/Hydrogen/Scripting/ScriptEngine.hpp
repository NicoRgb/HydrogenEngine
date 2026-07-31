#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include "ScriptBindings.hpp"
#include "Hydrogen/Scene/Scene.hpp"

namespace Hydrogen
{
	enum class ScriptFieldType { Float, Int, Bool, String, Entity, Unknown };
	using ScriptFieldValue = std::variant<double, int64_t, bool, std::string, Entity>;

	struct ScriptFieldMetadata
	{
		std::string Name;
		ScriptFieldType Type = ScriptFieldType::Unknown;
		ScriptFieldValue Value;
	};

	class ScriptInstance
	{
	public:
		virtual ~ScriptInstance() = default;

		virtual void OnCreate() = 0;
		virtual void OnUpdate(float dt) = 0;

		virtual void ParseMetadata(std::vector<ScriptFieldMetadata>& outFields) = 0;
		virtual void UpdateMetadata(const std::vector<ScriptFieldMetadata>& fields) = 0;
	};

	std::unique_ptr<ScriptInstance> CreateLuaScriptInstance(std::shared_ptr<class ScriptAsset> asset, class Entity entity);

	class ScriptModule
	{
	public:
		virtual ~ScriptModule() = default;
		virtual void RegisterBindings(class ScriptRegistry& registry) = 0;
	};

	class ScriptSystem
	{
	public:
		ScriptSystem(class Scene* scene)
			: m_Scene(scene) {}

		void IndexScripts();
		void OnInit();
		void OnUpdate(float dt);

	private:
		class Scene* m_Scene;
	};

	class ScriptEngine
	{
	public:
		static void Init();
	};
}
