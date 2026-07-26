#include "Hydrogen/Scene.hpp"
#include "Hydrogen/ScriptEngine.hpp"

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
