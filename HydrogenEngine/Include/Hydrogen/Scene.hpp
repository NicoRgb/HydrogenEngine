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
#include "Renderer/Texture.hpp"
#include "Renderer/VertexBuffer.hpp"
#include "Renderer/IndexBuffer.hpp"
#include "Hydrogen/Physics.hpp"

#include <random>

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

	class Scene : public std::enable_shared_from_this<Scene>
	{
	public:
		Scene();
		~Scene() = default;

		template <typename... Ts>
		void IterateComponents(auto func)
		{
			auto view = m_Registry.view<Ts...>();

			for (auto entityHandle : view)
			{
				Entity entity;
				entity.m_Entity = entityHandle;
				entity.m_Scene = shared_from_this();

				func(entity, view.get<Ts>(entityHandle)...);
			}
		}

		void CreateScripts();

		void UpdatePhysics(float timestep);
		void Update(float dt);

		json SerializeScene();
		void DeserializeScene(const json& j, AssetManager* assetManager);

		const PhysicsWorld& GetPhysicsWorld() { return m_PhysicsWorld; }

		void Clone(Scene& clone)
		{
		}

	private:
		entt::registry m_Registry;
		PhysicsWorld m_PhysicsWorld;
		ScriptSystem m_ScriptSystem;

		friend class Entity;
	};

	class Entity
	{
	public:
		Entity(const std::shared_ptr<Scene>& scene, std::string name);
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
		std::shared_ptr<Scene> m_Scene;

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

		static void OnImGuiRender(TagComponent& t)
		{
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			std::strncpy(buffer, t.Name.c_str(), sizeof(buffer) - 1);

			if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
			{
				t.Name = std::string(buffer);
			}
		}

		static void ToJson(json& j, const TagComponent& t)
		{
			j["name"] = t.Name;
		}

		static void FromJson(const json& j, TagComponent& t, AssetManager* assetManager)
		{
			t.Name = j.at("name");
		}
	};

	struct TransformComponent
	{
		TransformComponent(Entity entity)
		{
			(void)entity;
			Transform = glm::mat4(1.0f);
		}

		TransformComponent(Entity entity, glm::mat4 transform)
		{
			(void)entity;
			Transform = transform;
		}

		glm::mat4 Transform;

		glm::vec3 GetPosition() const
		{
			glm::vec3 p, r, s;
			TransformComponent::DecomposeTransform(Transform, p, r, s);
			return p;
		}

		void SetPosition(const glm::vec3& newPos)
		{
			glm::vec3 p, r, s;
			TransformComponent::DecomposeTransform(Transform, p, r, s);
			Transform = TransformComponent::RecomposeTransform(newPos, r, s);
		}

		glm::vec3 GetRotation() const
		{
			glm::vec3 p, r, s;
			TransformComponent::DecomposeTransform(Transform, p, r, s);
			return r;
		}

		void SetRotation(const glm::vec3& newRot)
		{
			glm::vec3 p, r, s;
			TransformComponent::DecomposeTransform(Transform, p, r, s);
			Transform = TransformComponent::RecomposeTransform(p, newRot, s);
		}

		glm::vec3 GetScale() const
		{
			glm::vec3 p, r, s;
			TransformComponent::DecomposeTransform(Transform, p, r, s);
			return s;
		}

		void SetScale(const glm::vec3& newScale)
		{
			glm::vec3 p, r, s;
			TransformComponent::DecomposeTransform(Transform, p, r, s);
			Transform = TransformComponent::RecomposeTransform(p, r, newScale);
		}

		static void OnImGuiRender(TransformComponent& transform)
		{
			if (ImGui::TreeNode("Transform"))
			{
				glm::vec3 translation, rotation, scale;
				DecomposeTransform(transform.Transform, translation, rotation, scale);

				bool changed = false;

				changed |= ImGui::DragFloat3("Translation", glm::value_ptr(translation), 0.1f);
				changed |= ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 0.1f);
				changed |= ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.1f);

				if (changed)
					transform.Transform = RecomposeTransform(translation, rotation, scale);

				ImGui::TreePop();
			}
		}

		static void ToJson(json& j, const TransformComponent& t)
		{
			glm::vec3 translation, rotation, scale;
			DecomposeTransform(t.Transform, translation, rotation, scale);

			j = json{ { "translation", { { "x", translation.x }, { "y", translation.y }, { "z", translation.z } } },
					  { "rotation", { { "x", rotation.x }, { "y", rotation.y }, { "z", rotation.z } } },
					  { "scale", { { "x", scale.x }, { "y", scale.y }, { "z", scale.z } } } };
		}

		static void FromJson(const json& j, TransformComponent& t, AssetManager* assetManager)
		{
			float translationX = j.at("translation").at("x").get<float>();
			float translationY = j.at("translation").at("y").get<float>();
			float translationZ = j.at("translation").at("z").get<float>();

			float rotationX = j.at("rotation").at("x").get<float>();
			float rotationY = j.at("rotation").at("y").get<float>();
			float rotationZ = j.at("rotation").at("z").get<float>();

			float scaleX = j.at("scale").at("x").get<float>();
			float scaleY = j.at("scale").at("y").get<float>();
			float scaleZ = j.at("scale").at("z").get<float>();

			glm::vec3 translation(translationX, translationY, translationZ);
			glm::vec3 rotation(rotationX, rotationY, rotationZ);
			glm::vec3 scale(scaleX, scaleY, scaleZ);

			t.Transform = RecomposeTransform(translation, rotation, scale);
		}

		static void DecomposeTransform(const glm::mat4& matrix, glm::vec3& translation, glm::quat& rotation, glm::vec3& scale)
		{
			using namespace glm;
			vec3 skew;
			vec4 perspective;

			decompose(matrix, scale, rotation, translation, skew, perspective);
		}

		static void DecomposeTransform(const glm::mat4& matrix, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
		{
			using namespace glm;
			vec3 skew;
			vec4 perspective;
			quat orientation;

			decompose(matrix, scale, orientation, translation, skew, perspective);

			rotation = glm::degrees(glm::eulerAngles(orientation));
		}

		static glm::mat4 RecomposeTransform(const glm::vec3& translation, const glm::quat& q, const glm::vec3& scale)
		{
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
				* glm::mat4_cast(q)
				* glm::scale(glm::mat4(1.0f), scale);

			return transform;
		}

		static glm::mat4 RecomposeTransform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
		{
			glm::quat q = glm::quat(glm::radians(rotation));

			glm::mat4 transform = glm::translate(glm::mat4(1.0f), translation)
				* glm::mat4_cast(q)
				* glm::scale(glm::mat4(1.0f), scale);

			return transform;
		}
	};

	struct MeshRendererComponent
	{
		MeshRendererComponent(Entity entity)
		{
			(void)entity;
		}

		std::shared_ptr<TextureAsset> Texture;
		std::shared_ptr<MeshAsset> Mesh;

		static void OnImGuiRender(MeshRendererComponent& t);

		static void ToJson(json& j, const MeshRendererComponent& t)
		{
			j = json{ { "Texture", std::filesystem::path(t.Texture->GetPath()).filename().string() },
					  { "Mesh", std::filesystem::path(t.Mesh->GetPath()).filename().string() } };
		}

		static void FromJson(const json& j, MeshRendererComponent& t, AssetManager* assetManager)
		{
			t.Texture = assetManager->GetAsset<TextureAsset>(j.at("Texture"));
			t.Mesh = assetManager->GetAsset<MeshAsset>(j.at("Mesh"));
		}
	};

	struct ScriptComponent
	{
		ScriptComponent(Entity entity) { (void)entity; }
		ScriptComponent(Entity entity, std::shared_ptr<ScriptAsset> script)
			: Script(script)
		{
		}

		std::shared_ptr<ScriptAsset> Script;
		sol::environment Environment;
		sol::load_result Chunk;

		static void OnImGuiRender(ScriptComponent& s);

		static void ToJson(json& j, const ScriptComponent& s)
		{
			j["Script"] = std::filesystem::path(s.Script->GetPath()).filename().string();
		}

		static void FromJson(const json& j, ScriptComponent& s, AssetManager* assetManager)
		{
			s.Script = assetManager->GetAsset<ScriptAsset>(j.at("Script"));
		}
	};
}
