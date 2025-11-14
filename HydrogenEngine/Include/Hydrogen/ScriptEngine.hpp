#pragma once

#include <sol/sol.hpp>
#include "Hydrogen/Scene.hpp"

namespace Hydrogen
{
    class ScriptEngine
    {
    public:
        static void Init()
        {
            s_Lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::table);

            BindEntity();
            BindComponents();
            BindMath();
        }

        static sol::state& GetLuaState() { return s_Lua; }

    private:
        static void BindEntity()
        {
            auto& lua = s_Lua;
            lua.new_usertype<Entity>("Entity",
                "GetUUID", &Entity::GetUUID,
                "GetTransform", &Entity::GetComponent<TransformComponent>
            );
        }

        static void BindComponents()
        {
            auto& lua = s_Lua;
            lua.new_usertype<TransformComponent>("TransformComponent",
                "pos", sol::property(&TransformComponent::GetPosition, &TransformComponent::SetPosition),
                "rot", sol::property(&TransformComponent::GetRotation, &TransformComponent::SetRotation),
                "scale", sol::property(&TransformComponent::GetScale, &TransformComponent::SetScale));
        }

        static void BindMath()
        {
            auto& lua = s_Lua;
            lua.new_usertype<glm::vec3>("vec3",
                sol::constructors<glm::vec3(), glm::vec3(float, float, float)>(),
                "x", &glm::vec3::x,
                "y", &glm::vec3::y,
                "z", &glm::vec3::z);
        }

    private:
        static inline sol::state s_Lua;
    };
}
