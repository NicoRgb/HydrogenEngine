#include "Hydrogen/Scene/Physics.hpp"
#include "Hydrogen/Scene/Components.hpp"
#include "Hydrogen/Application.hpp"

using namespace Hydrogen;

reactphysics3d::PhysicsCommon PhysicsWorld::PhysicsCommon;

PhysicsWorld::PhysicsWorld(Scene* scene, glm::vec3 gravity)
	: m_Scene(scene), m_PhysicsWorld(PhysicsCommon.createPhysicsWorld())
{
	HY_ASSERT(m_PhysicsWorld, "Failed to create physics world!");
	m_PhysicsWorld->setGravity({ gravity.x, gravity.y, gravity.z });
}

PhysicsWorld::~PhysicsWorld()
{
	if (m_PhysicsWorld)
	{
		PhysicsCommon.destroyPhysicsWorld(m_PhysicsWorld);
		m_PhysicsWorld = nullptr;
	}
}

void PhysicsWorld::Reset()
{
	m_Scene->IterateComponents<TransformComponent, RigidbodyComponent>([this](Entity entity, TransformComponent& transform, RigidbodyComponent& rb)
		{
			if (!rb.Rigidbody)
				return;

			m_PhysicsWorld->destroyRigidBody(rb.Rigidbody);
			rb.Rigidbody = nullptr;
		});

	for (uint32_t i = m_PhysicsWorld->getNbRigidBodies(); i > 0; --i)
	{
		HY_ENGINE_ERROR("dangling reactphysics3d::RigidBody found");
		reactphysics3d::RigidBody* body = m_PhysicsWorld->getRigidBody(i - 1);
		m_PhysicsWorld->destroyRigidBody(body);
	}
}

reactphysics3d::RigidBody* PhysicsWorld::CreateRigidbody(const TransformComponent& transform) const
{
	HY_ASSERT(m_PhysicsWorld, "Physics world is null!");

	glm::vec3 translation = transform.GetTranslation();
	glm::vec3 scale = transform.GetScale();
	glm::quat rotation = transform.GetRotation();

	if (glm::length(scale) < 0.001f)
	{
		scale = glm::vec3(1.0f);
		rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		HY_APP_WARN("Warning: Entity had 0 scale! Overriding to prevent NaN physics explosion.");
	}

	glm::quat normRot = glm::normalize(rotation);
	if (std::isnan(translation.x) || std::isnan(normRot.x))
	{
		HY_APP_ERROR("CRITICAL: NaN transform detected before RigidBody creation!");
		translation = glm::vec3(0.0f);
		normRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	}

	reactphysics3d::Vector3 position(translation.x, translation.y, translation.z);
	reactphysics3d::Quaternion orientation(normRot.x, normRot.y, normRot.z, normRot.w);

	reactphysics3d::Transform t(position, orientation);
	auto body = m_PhysicsWorld->createRigidBody(t);

	HY_ASSERT(body, "Failed to create rigidbody!");
	body->setType(reactphysics3d::BodyType::STATIC);

	return body;
}

void PhysicsWorld::DestroyRigidbody(reactphysics3d::RigidBody* body)
{
	if (m_PhysicsWorld && body)
	{
		m_PhysicsWorld->destroyRigidBody(body);
	}
}

void PhysicsWorld::UpdatePhysics(float timestep)
{
	if (m_PhysicsWorld)
	{
		m_PhysicsWorld->update(static_cast<reactphysics3d::decimal>(timestep));
	}
}

void PhysicsWorld::SyncTransforms()
{
	if (!m_PhysicsWorld || !m_Scene)
	{
		HY_ENGINE_ERROR("SyncTransform failure");
		return;
	}

	m_Scene->IterateComponents<TransformComponent, RigidbodyComponent>([](Entity entity, TransformComponent& transform, RigidbodyComponent& rb)
		{
			if (!rb.Rigidbody || (reactphysics3d::BodyType)rb.Type == reactphysics3d::BodyType::STATIC) return;

			const reactphysics3d::Transform& t = rb.Rigidbody->getTransform();

			reactphysics3d::Vector3 p = t.getPosition();
			reactphysics3d::Quaternion q = t.getOrientation();

			transform.SetTranslation(glm::vec3(p.x, p.y, p.z));
			transform.SetRotation(glm::quat(q.w, q.x, q.y, q.z));
		});
}

void RigidbodyComponent::ApplyRotationLock()
{
	if (!Rigidbody)
		return;

	Rigidbody->setLinearLockAxisFactor(reactphysics3d::Vector3(
		LockLinearX ? 0.0f : 1.0f,
		LockLinearY ? 0.0f : 1.0f,
		LockLinearZ ? 0.0f : 1.0f
	));

	Rigidbody->setAngularLockAxisFactor(reactphysics3d::Vector3(
		LockAngularX ? 0.0f : 1.0f,
		LockAngularY ? 0.0f : 1.0f,
		LockAngularZ ? 0.0f : 1.0f
	));
}

RigidbodyComponent::RigidbodyComponent(Entity entity)
	: GenericComponent(entity)
{
	Rigidbody = entity.GetScene()->GetPhysicsWorld().CreateRigidbody(entity.GetComponent<TransformComponent>());
}

void RigidbodyComponent::SetType(reactphysics3d::BodyType type)
{
	Type = (int)type;
	if (Rigidbody)
	{
		Rigidbody->setType(type);
		if (type == reactphysics3d::BodyType::DYNAMIC)
			Rigidbody->updateLocalInertiaTensorFromColliders();
	}
}

void RigidbodyComponent::SetMass(float mass)
{
	Mass = glm::max(mass, 0.001f);
	if (Rigidbody)
	{
		Rigidbody->setMass(Mass);
		if ((reactphysics3d::BodyType)Type == reactphysics3d::BodyType::DYNAMIC)
			Rigidbody->updateLocalInertiaTensorFromColliders();
	}
}

void RigidbodyComponent::SetUseGravity(bool useGravity)
{
	UseGravity = useGravity;
	if (Rigidbody) Rigidbody->enableGravity(UseGravity);
}

void RigidbodyComponent::ApplyForce(glm::vec3 force)
{
	if (Rigidbody && (reactphysics3d::BodyType)Type == reactphysics3d::BodyType::DYNAMIC)
		Rigidbody->applyLocalForceAtCenterOfMass({ force.x, force.y, force.z });
}

void RigidbodyComponent::ApplyTorque(glm::vec3 torque)
{
	if (Rigidbody && (reactphysics3d::BodyType)Type == reactphysics3d::BodyType::DYNAMIC)
		Rigidbody->applyLocalTorque({ torque.x, torque.y, torque.z });
}

void RigidbodyComponent::SetLinearVelocity(glm::vec3 velocity)
{
	if (Rigidbody) Rigidbody->setLinearVelocity({ velocity.x, velocity.y, velocity.z });
}

void RigidbodyComponent::SetAngularVelocity(glm::vec3 velocity)
{
	if (Rigidbody) Rigidbody->setAngularVelocity({ velocity.x, velocity.y, velocity.z });
}

void RigidbodyComponent::Deserialize(const json& j)
{
	GenericComponent::Deserialize(j);
	if (Rigidbody)
	{
		Rigidbody->setType((reactphysics3d::BodyType)Type);
		Rigidbody->setMass(Mass);
		Rigidbody->setLinearDamping(LinearDamping);
		Rigidbody->setAngularDamping(AngularDamping);
		Rigidbody->enableGravity(UseGravity);

		Rigidbody->setLinearLockAxisFactor(reactphysics3d::Vector3(
			LockLinearX ? 0.0f : 1.0f,
			LockLinearY ? 0.0f : 1.0f,
			LockLinearZ ? 0.0f : 1.0f
		));

		Rigidbody->setAngularLockAxisFactor(reactphysics3d::Vector3(
			LockAngularX ? 0.0f : 1.0f,
			LockAngularY ? 0.0f : 1.0f,
			LockAngularZ ? 0.0f : 1.0f
		));

		if ((reactphysics3d::BodyType)Type == reactphysics3d::BodyType::DYNAMIC)
		{
			Rigidbody->updateLocalInertiaTensorFromColliders();
			Rigidbody->setLocalCenterOfMass(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
		}
	}
	else
	{
		//createrigidbody
	}
}

ColliderComponent::ColliderComponent(Entity entity)
	: GenericComponent(entity), Rigidbody(entity.TryGetComponent<RigidbodyComponent>()), Collider(nullptr)
{
	CreateCollider();
}

void ColliderComponent::DestroyCollider()
{
	if (Rigidbody && Rigidbody->Rigidbody && Collider)
	{
		Rigidbody->Rigidbody->removeCollider(Collider);
		Collider = nullptr;
	}
}

void ColliderComponent::CreateCollider()
{
	if (!Rigidbody || !Rigidbody->Rigidbody) return;

	auto body = Rigidbody->Rigidbody;

	DestroyCollider();

	if (glm::length(LocalRotation) < 0.001f)
	{
		LocalRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	}
	glm::quat normRot = glm::normalize(LocalRotation);

	reactphysics3d::Vector3 position(LocalPosition.x, LocalPosition.y, LocalPosition.z);
	reactphysics3d::Quaternion orientation(normRot.x, normRot.y, normRot.z, normRot.w);
	reactphysics3d::Transform localTransform(position, orientation);

	if ((ColliderComponent::Type)ColliderType == Type::Box)
	{
		glm::vec3 safeSize = glm::max(Size, glm::vec3(0.01f));
		reactphysics3d::Vector3 halfExtents(safeSize.x * 0.5f, safeSize.y * 0.5f, safeSize.z * 0.5f);
		Collider = body->addCollider(PhysicsWorld::PhysicsCommon.createBoxShape(halfExtents), localTransform);
	}
	else if ((ColliderComponent::Type)ColliderType == Type::Sphere)
	{
		float safeRadius = glm::max(Radius, 0.01f);
		Collider = body->addCollider(PhysicsWorld::PhysicsCommon.createSphereShape(safeRadius), localTransform);
	}
	else if ((ColliderComponent::Type)ColliderType == Type::Capsule)
	{
		float safeRadius = glm::max(Radius, 0.01f);
		float safeHeight = glm::max(Height, 0.01f);
		Collider = body->addCollider(PhysicsWorld::PhysicsCommon.createCapsuleShape(safeRadius, safeHeight), localTransform);
	}

	if ((reactphysics3d::BodyType)Rigidbody->Type == reactphysics3d::BodyType::DYNAMIC)
	{
		body->setMass(glm::max(Rigidbody->Mass, 0.001f));

		body->updateLocalInertiaTensorFromColliders();
		body->setLocalCenterOfMass(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
	}

	body->setLinearVelocity(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
	body->setAngularVelocity(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
}

void ColliderComponent::Deserialize(const json& j)
{
	GenericComponent::Deserialize(j);
	CreateCollider();
}
