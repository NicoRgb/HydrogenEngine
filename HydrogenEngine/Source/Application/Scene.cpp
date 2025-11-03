#include "Hydrogen/Scene.hpp"

using namespace Hydrogen;

Entity::Entity(const std::shared_ptr<Scene>& scene)
	: m_Scene(scene)
{
	m_Entity = m_Scene->m_Registry.create();
	AddComponent<TransformComponent>(glm::mat4(1.0f));
}

Entity::~Entity()
{
	m_Scene->m_Registry.destroy(m_Entity);
}
