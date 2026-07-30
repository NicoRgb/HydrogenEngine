#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include "ScriptBindings.hpp"

namespace Hydrogen
{
	enum class ScriptFieldType { Float, Int, Bool, String };
	struct ScriptFieldMetadata { std::string Name; ScriptFieldType Type; std::vector<char> Buffer; };

	class ScriptInstance
	{
	public:
		virtual ~ScriptInstance() = default;

		virtual void OnCreate() = 0;
		virtual void OnUpdate(float dt) = 0;
		virtual void ParseMetadata(std::unordered_map<std::string, ScriptFieldMetadata>& outFields) = 0;
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
