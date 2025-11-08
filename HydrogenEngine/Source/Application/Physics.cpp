#include "Hydrogen/Physics.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Application.hpp"
#include <glm/glm.hpp>

using namespace Hydrogen;

reactphysics3d::PhysicsCommon PhysicsWorld::s_PhysicsCommon;

PhysicsWorld::PhysicsWorld(Scene* scene, glm::vec3 gravity)
	: m_Scene(scene), m_PhysicsWorld(s_PhysicsCommon.createPhysicsWorld())
{
	m_PhysicsWorld->setGravity({ gravity.x, gravity.y, gravity.z });
}

PhysicsWorld::~PhysicsWorld()
{
}

reactphysics3d::RigidBody* PhysicsWorld::CreateRigidbody(const TransformComponent& transform) const
{
	glm::vec3 translation, scale;
	glm::quat rotation;
	TransformComponent::DecomposeTransform(transform.Transform, translation, rotation, scale);

	reactphysics3d::Vector3 position(translation.x, translation.y, translation.z);
	reactphysics3d::Quaternion orientation(rotation.x, rotation.y, rotation.z, rotation.w);
	reactphysics3d::Transform t(position, orientation);

	return m_PhysicsWorld->createRigidBody(t);
}

void PhysicsWorld::Update(float timestep)
{
	m_PhysicsWorld->update((reactphysics3d::decimal)timestep);
	m_Scene->IterateComponents<TransformComponent, RigidbodyComponent>([&](Entity entity, TransformComponent& transform, const RigidbodyComponent& rb)
		{
			(void)entity;
			const reactphysics3d::Transform& t = rb.Rigidbody->getTransform();

			glm::vec3 translation, rotation, scale;
			TransformComponent::DecomposeTransform(transform.Transform, translation, rotation, scale);

			transform.Transform = TransformComponent::RecomposeTransform({ t.getPosition().x, t.getPosition().y, t.getPosition().z },
																		 glm::quat(t.getOrientation().x, t.getOrientation().y, t.getOrientation().z, t.getOrientation().w),
																		 scale);
		});
}

RigidbodyComponent::RigidbodyComponent(Entity entity)
{
	Rigidbody = Application::Get()->CurrentScene->GetScene()->GetPhysicsWorld().CreateRigidbody(entity.GetComponent<TransformComponent>());
}
