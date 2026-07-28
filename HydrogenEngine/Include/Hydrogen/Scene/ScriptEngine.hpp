#pragma once

#include <sol/sol.hpp>

namespace Hydrogen
{
	class ScriptSystem
	{
	public:
		ScriptSystem() = default;
		ScriptSystem(class Scene* scene)
			: m_Scene(scene)
		{
		}

		void OnCreate();
		void OnUpdate(float dt);

	private:
		class Scene* m_Scene;
	};

	class ScriptEngine
	{
	public:
		static void Init();
		static sol::state& GetLuaState() { return s_Lua; }

	private:
		static void BindEntity();
		static void BindComponents();
		static void BindMath();
		static void BindInputSystem();
		static void BindPhysics();
		static void BindLogging();

	private:
		static inline sol::state s_Lua;
	};
}
