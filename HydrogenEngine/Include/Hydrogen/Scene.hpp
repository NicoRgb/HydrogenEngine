#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "AssetManager.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/VertexBuffer.hpp"
#include "Renderer/IndexBuffer.hpp"

namespace Hydrogen
{
	class Scene : public std::enable_shared_from_this<Scene>
	{
	public:
		Scene() = default;
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

	private:
		entt::registry m_Registry;

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

		void Delete();

		template <typename T, typename... Args>
		void AddComponent(Args&&... args)
		{
			m_Scene->m_Registry.emplace<T>(m_Entity, std::forward<Args>(args)...);
		}

		template <typename T>
		T& GetComponent()
		{
			return m_Scene->m_Registry.get<T>(m_Entity);
		}

	private:
		entt::entity m_Entity;
		std::shared_ptr<Scene> m_Scene;

		friend class Scene;
	};

	struct TagComponent
	{
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
	};

	struct TransformComponent
	{
		glm::mat4 Transform;

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

	private:
		static void DecomposeTransform(const glm::mat4& matrix, glm::vec3& translation, glm::vec3& rotation, glm::vec3& scale)
		{
			using namespace glm;
			vec3 skew;
			vec4 perspective;
			quat orientation;

			decompose(matrix, scale, orientation, translation, skew, perspective);

			rotation = degrees(eulerAngles(conjugate(orientation)));
		}

		static glm::mat4 RecomposeTransform(const glm::vec3& translation, const glm::vec3& rotation, const glm::vec3& scale)
		{
			glm::quat q = glm::quat(glm::radians(rotation));

			glm::mat4 translationMat = glm::translate(glm::mat4(1.0f), translation);
			glm::mat4 rotationMat = glm::toMat4(q);
			glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);

			return translationMat * rotationMat * scaleMat;
		}
	};

	struct MeshRendererComponent
	{
		//const std::shared_ptr<TextureAsset> Texture;
		//const std::shared_ptr<MeshAsset> Mesh;

		const std::shared_ptr<VertexBuffer> VertexBuf;
		const std::shared_ptr<IndexBuffer> IndexBuf;

		const std::shared_ptr<Texture> _Texture;

		static void OnImGuiRender(MeshRendererComponent& t)
		{
			if (ImGui::TreeNode("Mesh Renderer"))
			{
				ImGui::TreePop();
			}
		}
	};
}
