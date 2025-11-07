#include "Hydrogen/Physics.hpp"

using namespace Hydrogen;

reactphysics3d::PhysicsCommon PhysicsWorld::s_PhysicsCommon;

PhysicsWorld::PhysicsWorld(const std::shared_ptr<Scene>& scene, glm::vec3 gravity)
	: m_Scene(scene), m_PhysicsWorld(s_PhysicsCommon.createPhysicsWorld())
{
	m_PhysicsWorld->setGravity({ gravity.x, gravity.y, gravity.z });
}

PhysicsWorld::~PhysicsWorld()
{
	s_PhysicsCommon.destroyPhysicsWorld(m_PhysicsWorld);
}

reactphysics3d::RigidBody* PhysicsWorld::CreateRigidbody(const TransformComponent& transform)
{
	glm::vec3 translation, scale;
	glm::quat rotation;
	TransformComponent::DecomposeTransform(transform.Transform, translation, rotation, scale);

	reactphysics3d::Vector3 position(translation.x, translation.y, translation.z);
	reactphysics3d::Quaternion orientation(rotation.x, rotation.y, rotation.z, rotation.w);
	reactphysics3d::Transform t(position, orientation);

	return m_PhysicsWorld->createRigidBody(t);
}

void PhysicsWorld::Update(double timestep)
{
	m_PhysicsWorld->update(timestep);
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
