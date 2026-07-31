#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <sol/sol.hpp>

#include "Physics.hpp"

#include <random>
#include <memory>

namespace Hydrogen
{
	class Entity;

	struct UUIDComponent
	{
		UUIDComponent(class Entity);
		UUIDComponent(class Entity, uint64_t uuid);

		uint64_t UUID;

		uint64_t GenerateUUID()
		{
			static std::random_device rd;
			static std::mt19937_64 gen(rd());
			static std::uniform_int_distribution<uint64_t> dis;
			return dis(gen);
		}
	};

	class Scene
	{
	public:
		Scene();
		~Scene() = default;

		template<typename... Ts, typename Func>
		void IterateComponents(Func&& func)
		{
			if constexpr (sizeof...(Ts) == 0)
			{
				auto view = m_Registry.view<entt::entity>();

				for (auto entityHandle : view)
				{
					Entity entity;
					entity.m_Entity = entityHandle;
					entity.m_Scene = this;

					std::forward<Func>(func)(entity);
				}
			}
			else
			{
				auto view = m_Registry.view<Ts...>();

				for (auto entityHandle : view)
				{
					Entity entity;
					entity.m_Entity = entityHandle;
					entity.m_Scene = this;

					std::forward<Func>(func)(
						entity,
						view.template get<Ts>(entityHandle)...
						);
				}
			}
		}

		Entity GetEntityByEntityID(uint32_t id);
		Entity GetEntityByUUID(uint64_t uuid);

		void UpdatePhysics(float timestep);
		void IndexScripts();
		void InitScripts();
		void Update(float dt);

		void ResetPhysics() { m_PhysicsWorld.Reset(); }

		json SerializeScene();
		void DeserializeScene(const json& j);

		PhysicsWorld& GetPhysicsWorld() { return m_PhysicsWorld; }

		void Clone(Scene& clone)
		{
		}

		entt::registry& GetRegistry() { return m_Registry; }
		
	private:
		entt::registry m_Registry;
		PhysicsWorld m_PhysicsWorld;
		std::unique_ptr<class ScriptSystem> m_ScriptSystem;

		friend class Entity;
	};

	class Entity
	{
	public:
		Entity(Scene* scene, std::string name);
		Entity(entt::entity id, Scene* scene);
		Entity();
		~Entity() = default;

		Scene* GetScene() { return m_Scene; }

		bool operator==(const Entity& other) const noexcept
		{
			return m_Entity == other.m_Entity && m_Scene == other.m_Scene;
		}

		bool operator!=(const Entity& other) const noexcept
		{
			return !(*this == other);
		}

		bool IsValid() const
		{
			return m_Entity != entt::null && m_Scene->m_Registry.valid(m_Entity);
		}

		uint64_t GetUUID() const;
		void SetUUID(uint64_t uuid);

		uint32_t GetID() const { return (uint32_t)m_Entity; }

		void Delete();

		template <typename T, typename... Args>
		void AddComponent(Args&&... args)
		{
			m_Scene->m_Registry.emplace<T>(m_Entity, *this, std::forward<Args>(args)...);
		}

		template <typename T>
		const T& GetComponent() const
		{
			return m_Scene->m_Registry.get<T>(m_Entity);
		}

		template <typename T>
		T& GetComponent()
		{
			return m_Scene->m_Registry.get<T>(m_Entity);
		}

		template <typename T>
		T* TryGetComponent()
		{
			return m_Scene->m_Registry.try_get<T>(m_Entity);
		}

		template <typename T>
		void RemoveComponent()
		{
			m_Scene->m_Registry.remove<T>(m_Entity);
		}

		template <typename T>
		bool HasComponent()
		{
			return m_Scene->m_Registry.any_of<T>(m_Entity);
		}

		template <typename T, typename... Args>
		T& GetOrAddComponent(Args&&... args)
		{
			return m_Scene->m_Registry.get_or_emplace<T>(m_Entity, *this, std::forward<Args>(args)...);
		}

	private:
		entt::entity m_Entity;
		Scene* m_Scene;

		friend class Scene;
	};
}
