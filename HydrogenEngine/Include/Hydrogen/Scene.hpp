#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <sol/sol.hpp>

#include "AssetManager.hpp"
#include "Hydrogen/Physics.hpp"

#include <random>
#include <memory>

namespace Hydrogen
{
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

	class Entity;

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

		void CreateScripts();

		void UpdatePhysics(float timestep);
		void Update(float dt);

		void ResetPhysics() { m_PhysicsWorld.Reset(); }

		json SerializeScene();
		void DeserializeScene(const json& j, AssetManager* assetManager);

		PhysicsWorld& GetPhysicsWorld() { return m_PhysicsWorld; }

		void Clone(Scene& clone)
		{
		}

		entt::registry& GetRegistry() { return m_Registry; }

	private:
		entt::registry m_Registry;
		PhysicsWorld m_PhysicsWorld;
		ScriptSystem m_ScriptSystem;

		friend class Entity;
	};

	class Entity
	{
	public:
		Entity(Scene* scene, std::string name);
		Entity(entt::entity id, Scene* scene);
		Entity();
		~Entity() = default;

		bool operator==(const Entity& other) const noexcept
		{
			return m_Entity == other.m_Entity && m_Scene == other.m_Scene;
		}

		bool operator!=(const Entity& other) const noexcept
		{
			return !(*this == other);
		}

		bool IsValid()
		{
			return m_Entity != entt::null && m_Scene->m_Registry.valid(m_Entity);
		}

		uint64_t GetUUID();
		void SetUUID(uint64_t uuid);

		uint32_t GetID() const { return (uint32_t)m_Entity; }

		void Delete();

		template <typename T, typename... Args>
		void AddComponent(Args&&... args)
		{
			m_Scene->m_Registry.emplace<T>(m_Entity, *this, std::forward<Args>(args)...);
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

	private:
		entt::entity m_Entity;
		Scene* m_Scene;

		friend class Scene;
	};

	struct TagComponent
	{
		TagComponent(Entity entity)
		{
			(void)entity;
		}

		TagComponent(Entity entity, std::string name)
		{
			(void)entity;
			Name = name;
		}

		std::string Name;

		static void ToJson(json& j, const TagComponent& t)
		{
			j["name"] = t.Name;
		}

		static void FromJson(const json& j, TagComponent& t, AssetManager* assetManager)
		{
			t.Name = j.at("name");
		}
	};

	struct RelationshipComponent
	{
		RelationshipComponent(Entity entity)
		{
			(void)entity;
		}

		RelationshipComponent(Entity entity, uint64_t parentUUID)
		{
			(void)entity;
			ParentUUID = parentUUID;
		}

		uint64_t ParentUUID;

		static void ToJson(json& j, const RelationshipComponent& t)
		{
			j["parent"] = t.ParentUUID;
		}

		static void FromJson(const json& j, RelationshipComponent& t, AssetManager* assetManager)
		{
			t.ParentUUID = j.value("parent", 0);
		}
	};

	struct TransformComponent
	{
		TransformComponent(Entity entity)
		{
			(void)entity;
			Translation = glm::vec3(0.0f, 0.0f, 0.0f);
			Rotation = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
			Scale = glm::vec3(1.0f, 1.0f, 1.0f);
		}

		const glm::mat4& GetModel()
		{
			if (Dirty)
			{
				Dirty = false;

				ModelCache = glm::mat4(1.0f);
				ModelCache = glm::translate(ModelCache, Translation);
				ModelCache = ModelCache * glm::mat4_cast(Rotation);
				ModelCache = glm::scale(ModelCache, Scale);
			}

			return ModelCache;
		}

		glm::vec3 GetTranslation() const
		{
			return Translation;
		}

		void SetTranslation(const glm::vec3& newTranslation)
		{
			Translation = newTranslation;
			Dirty = true;
		}

		glm::quat GetRotation() const
		{
			return Rotation;
		}

		void SetRotation(const glm::quat& newRotation)
		{
			Rotation = newRotation;
			Dirty = true;
		}

		glm::vec3 GetScale() const
		{
			return Scale;
		}

		void SetScale(const glm::vec3& newScale)
		{
			Scale = newScale;
			Dirty = true;
		}

		static void ToJson(json& j, const TransformComponent& t)
		{
			j = json{ { "translation", { { "x", t.Translation.x }, { "y", t.Translation.y }, { "z", t.Translation.z } } },
					  { "rotation", { { "x", t.Rotation.x}, {"y", t.Rotation.y}, {"z", t.Rotation.z}, { "w", t.Rotation.w } }},
					  { "scale", { { "x", t.Scale.x }, { "y", t.Scale.y }, { "z", t.Scale.z } } } };
		}

		static void FromJson(const json& j, TransformComponent& t, AssetManager* assetManager)
		{
			float translationX = j.at("translation").at("x").get<float>();
			float translationY = j.at("translation").at("y").get<float>();
			float translationZ = j.at("translation").at("z").get<float>();

			float rotationX = j.at("rotation").at("x").get<float>();
			float rotationY = j.at("rotation").at("y").get<float>();
			float rotationZ = j.at("rotation").at("z").get<float>();
			float rotationW = j.at("rotation").at("w").get<float>();

			float scaleX = j.at("scale").at("x").get<float>();
			float scaleY = j.at("scale").at("y").get<float>();
			float scaleZ = j.at("scale").at("z").get<float>();

			t.Translation = glm::vec3(translationX, translationY, translationZ);
			t.Rotation = glm::quat(rotationW, rotationX, rotationY, rotationZ);
			t.Scale = glm::vec3(scaleX, scaleY, scaleZ);
		}

	private:
		glm::vec3 Translation;
		glm::quat Rotation;
		glm::vec3 Scale;

		bool Dirty = true;
		glm::mat4 ModelCache;
	};

	struct MeshRendererComponent
	{
		MeshRendererComponent(Entity entity);

		std::shared_ptr<StaticMeshAsset> Mesh;
		std::shared_ptr<MaterialAsset> Material;

		static void ToJson(json& j, const MeshRendererComponent& t)
		{
			j = json();
			if (t.Material)
				j["Material"] = std::filesystem::path(t.Material->GetPath()).filename().string();
			if (t.Mesh)
				j["Mesh"] = std::filesystem::path(t.Mesh->GetPath()).filename().string();
		}

		static void FromJson(const json& j, MeshRendererComponent& t, AssetManager* assetManager)
		{
			auto materialPath = j.value("Material", "");
			auto meshPath = j.value("Mesh", "");

			if (!materialPath.empty())
			{
				t.Material = assetManager->GetAsset<MaterialAsset>(materialPath);
			}
			if (!meshPath.empty())
			{
				t.Mesh = assetManager->GetAsset<StaticMeshAsset>(meshPath);
			}
		}
	};

	struct DirectionalLightComponent
	{
		DirectionalLightComponent(Entity entity)
		{
			(void)entity;
		}

		glm::vec3 Color;
		float Intensity;

		static void ToJson(json& j, const DirectionalLightComponent& t)
		{
			j = json{ { "Color", { { "r", t.Color.r }, { "g", t.Color.g }, { "b", t.Color.b } } },
					  { "Intensity", t.Intensity } };
		}

		static void FromJson(const json& j, DirectionalLightComponent& t, AssetManager* assetManager)
		{	
			const auto& color = j.value("Color", nlohmann::json::object());

			float r = color.value("r", 1.0f);
			float g = color.value("g", 1.0f);
			float b = color.value("b", 1.0f);
			t.Color = glm::vec3(r, g, b);

			t.Intensity = j.value("Intensity", 1.0f);
		}
	};

	struct PointLightComponent
	{
		PointLightComponent(Entity entity)
		{
			(void)entity;
		}

		glm::vec3 Color;
		float Intensity;
		float Radius;

		static void ToJson(json& j, const PointLightComponent& t)
		{
			j = json{ { "Color", { { "r", t.Color.r }, { "g", t.Color.g }, { "b", t.Color.b } } },
					  { "Intensity", t.Intensity },
					  { "Radius", t.Radius } };
		}

		static void FromJson(const json& j, PointLightComponent& t, AssetManager* assetManager)
		{
			const auto& color = j.value("Color", nlohmann::json::object());

			float r = color.value("r", 1.0f);
			float g = color.value("g", 1.0f);
			float b = color.value("b", 1.0f);
			t.Color = glm::vec3(r, g, b);

			t.Intensity = j.value("Intensity", 1.0f);
			t.Radius = j.value("Radius", 1.0f);
		}
	};

	struct ScriptDesc
	{
		ScriptDesc()
		{
			Script = nullptr;
		}

		std::shared_ptr<ScriptAsset> Script;
		sol::environment Environment;
		sol::load_result Chunk;
	};

	struct ScriptsComponent
	{
		ScriptsComponent(Entity entity) { (void)entity; }
		ScriptsComponent(const ScriptsComponent&) = delete;
		ScriptsComponent& operator=(const ScriptsComponent&) = delete;
		ScriptsComponent(ScriptsComponent&&) = default;
		ScriptsComponent& operator=(ScriptsComponent&&) = default;

		std::vector<std::unique_ptr<ScriptDesc>> Scripts;

		static void ToJson(json& j, const ScriptsComponent& s)
		{
			j = json();
			j["Scripts"] = json::array();

			for (const auto& scriptItem : s.Scripts)
			{
				if (scriptItem->Script)
				{
					std::string filename = std::filesystem::path(scriptItem->Script->GetPath()).filename().string();
					j["Scripts"].push_back(filename);
				}
			}
		}

		static void FromJson(const json& j, ScriptsComponent& s, AssetManager* assetManager)
		{
			s.Scripts.clear();

			if (j.contains("Scripts") && j["Scripts"].is_array())
			{
				for (const auto& scriptPathJson : j["Scripts"])
				{
					std::string scriptPath = scriptPathJson.get<std::string>();
					if (!scriptPath.empty())
					{
						auto script = std::make_unique<ScriptDesc>();
						script->Script = assetManager->GetAsset<ScriptAsset>(scriptPath);
						s.Scripts.push_back(std::move(script));
					}
				}
			}
		}
	};
}
