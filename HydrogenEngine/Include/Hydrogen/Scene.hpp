#pragma once

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "AssetManager.hpp"
#include "Renderer/Texture.hpp"
#include "Renderer/VertexBuffer.hpp"
#include "Renderer/IndexBuffer.hpp"

namespace Hydrogen
{
	class Scene
	{
	public:
		Scene() = default;
		~Scene() = default;

		template <typename... Ts>
		void IterateComponents(auto func)
		{
			auto view = m_Registry.view<Ts...>();
			view.each(func);
		}

	private:
		entt::registry m_Registry;

		friend class Entity;
	};

	class Entity
	{
	public:
		Entity(const std::shared_ptr<Scene>& scene);
		~Entity();

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
	};

	struct TransformComponent
	{
		glm::mat4 Transform;
	};

	struct MeshRendererComponent
	{
		//const std::shared_ptr<TextureAsset> Texture;
		//const std::shared_ptr<MeshAsset> Mesh;

		const std::shared_ptr<VertexBuffer> VertexBuf;
		const std::shared_ptr<IndexBuffer> IndexBuf;

		const std::shared_ptr<Texture> _Texture;
	};
}
