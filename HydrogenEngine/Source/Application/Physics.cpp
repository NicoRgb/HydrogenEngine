#include "Hydrogen/Physics.hpp"
#include "Hydrogen/Scene.hpp"
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
	for (uint32_t i = m_PhysicsWorld->getNbRigidBodies(); i > 0; --i)
	{
		reactphysics3d::RigidBody* body = m_PhysicsWorld->getRigidBody(i - 1);
		m_PhysicsWorld->destroyRigidBody(body);
	}
}

reactphysics3d::RigidBody* PhysicsWorld::CreateRigidbody(const TransformComponent& transform) const
{
	HY_ASSERT(m_PhysicsWorld, "Physics world is null!");

	glm::vec3 translation, scale;
	glm::quat rotation;
	TransformComponent::DecomposeTransform(transform.Transform, translation, rotation, scale);

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
	if (!m_PhysicsWorld || !m_Scene) return;

	m_Scene->IterateComponents<TransformComponent, RigidbodyComponent>([](Entity entity, TransformComponent& transform, RigidbodyComponent& rb)
		{
			if (!rb.Rigidbody || rb.Type == reactphysics3d::BodyType::STATIC) return;

			const reactphysics3d::Transform& t = rb.Rigidbody->getTransform();

			glm::vec3 translation, scale;
			glm::quat rotation;
			TransformComponent::DecomposeTransform(transform.Transform, translation, rotation, scale);

			reactphysics3d::Vector3 p = t.getPosition();
			reactphysics3d::Quaternion q = t.getOrientation();

			transform.Transform = TransformComponent::RecomposeTransform(
				glm::vec3(p.x, p.y, p.z),
				glm::quat(q.w, q.x, q.y, q.z),
				scale
			);
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
{
	Rigidbody = Application::Get()->CurrentScene->GetScene()->GetPhysicsWorld().CreateRigidbody(entity.GetComponent<TransformComponent>());
}

void RigidbodyComponent::SetType(reactphysics3d::BodyType type)
{
	Type = type;
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
		if (Type == reactphysics3d::BodyType::DYNAMIC)
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
	if (Rigidbody && Type == reactphysics3d::BodyType::DYNAMIC)
		Rigidbody->applyLocalForceAtCenterOfMass({ force.x, force.y, force.z });
}

void RigidbodyComponent::ApplyTorque(glm::vec3 torque)
{
	if (Rigidbody && Type == reactphysics3d::BodyType::DYNAMIC)
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

void RigidbodyComponent::ToJson(json& j, const RigidbodyComponent& rb)
{
	j = json{
		{ "type", static_cast<int>(rb.Type) },
		{ "mass", rb.Mass },
		{ "linearDampening", rb.LinearDamping },
		{ "angularDampening", rb.AngularDamping },
		{ "gravity", rb.UseGravity },
		{ "linearLock", { rb.LockLinearX, rb.LockLinearY, rb.LockLinearZ } },
		{ "angularLock", { rb.LockAngularX, rb.LockAngularY, rb.LockAngularZ } }
	};
}

void RigidbodyComponent::FromJson(const json& j, RigidbodyComponent& rb, AssetManager* assetManager)
{
	rb.Type = static_cast<reactphysics3d::BodyType>(j.value("type", 0));
	rb.Mass = j.value("mass", 1.0f);
	rb.LinearDamping = j.value("linearDampening", 0.0f);
	rb.AngularDamping = j.value("angularDampening", 0.0f);
	rb.UseGravity = j.value("gravity", true);

	if (j.contains("linearLock") && j["linearLock"].is_array() && j["linearLock"].size() == 3)
	{
		rb.LockLinearX = j["linearLock"][0].get<bool>();
		rb.LockLinearY = j["linearLock"][1].get<bool>();
		rb.LockLinearZ = j["linearLock"][2].get<bool>();
	}

	if (j.contains("angularLock") && j["angularLock"].is_array() && j["angularLock"].size() == 3)
	{
		rb.LockAngularX = j["angularLock"][0].get<bool>();
		rb.LockAngularY = j["angularLock"][1].get<bool>();
		rb.LockAngularZ = j["angularLock"][2].get<bool>();
	}

	if (rb.Rigidbody)
	{
		rb.Rigidbody->setType(rb.Type);
		rb.Rigidbody->setMass(rb.Mass);
		rb.Rigidbody->setLinearDamping(rb.LinearDamping);
		rb.Rigidbody->setAngularDamping(rb.AngularDamping);
		rb.Rigidbody->enableGravity(rb.UseGravity);

		rb.Rigidbody->setLinearLockAxisFactor(reactphysics3d::Vector3(
			rb.LockLinearX ? 0.0f : 1.0f,
			rb.LockLinearY ? 0.0f : 1.0f,
			rb.LockLinearZ ? 0.0f : 1.0f
		));

		rb.Rigidbody->setAngularLockAxisFactor(reactphysics3d::Vector3(
			rb.LockAngularX ? 0.0f : 1.0f,
			rb.LockAngularY ? 0.0f : 1.0f,
			rb.LockAngularZ ? 0.0f : 1.0f
		));

		if (rb.Type == reactphysics3d::BodyType::DYNAMIC)
		{
			rb.Rigidbody->updateLocalInertiaTensorFromColliders();
			rb.Rigidbody->setLocalCenterOfMass(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
		}
	}
}

ColliderComponent::ColliderComponent(Entity entity)
	: Rigidbody(entity.TryGetComponent<RigidbodyComponent>()), Collider(nullptr)
{
	CreateCollider(*this);
}

void ColliderComponent::DestroyCollider()
{
	if (Rigidbody && Rigidbody->Rigidbody && Collider)
	{
		Rigidbody->Rigidbody->removeCollider(Collider);
		Collider = nullptr;
	}
}

void ColliderComponent::CreateCollider(ColliderComponent& col)
{
	if (!Rigidbody || !Rigidbody->Rigidbody) return;

	auto body = Rigidbody->Rigidbody;

	col.DestroyCollider();

	if (glm::length(col.LocalRotation) < 0.001f)
	{
		col.LocalRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	}
	glm::quat normRot = glm::normalize(col.LocalRotation);

	reactphysics3d::Vector3 position(col.LocalPosition.x, col.LocalPosition.y, col.LocalPosition.z);
	reactphysics3d::Quaternion orientation(normRot.x, normRot.y, normRot.z, normRot.w);
	reactphysics3d::Transform localTransform(position, orientation);

	if (col.ColliderType == Type::Box)
	{
		glm::vec3 safeSize = glm::max(col.Size, glm::vec3(0.01f));
		reactphysics3d::Vector3 halfExtents(safeSize.x * 0.5f, safeSize.y * 0.5f, safeSize.z * 0.5f);
		col.Collider = body->addCollider(PhysicsWorld::PhysicsCommon.createBoxShape(halfExtents), localTransform);
	}
	else if (col.ColliderType == Type::Sphere)
	{
		float safeRadius = glm::max(col.Radius, 0.01f);
		col.Collider = body->addCollider(PhysicsWorld::PhysicsCommon.createSphereShape(safeRadius), localTransform);
	}
	else if (col.ColliderType == Type::Capsule)
	{
		float safeRadius = glm::max(col.Radius, 0.01f);
		float safeHeight = glm::max(col.Height, 0.01f);
		col.Collider = body->addCollider(PhysicsWorld::PhysicsCommon.createCapsuleShape(safeRadius, safeHeight), localTransform);
	}

	if (Rigidbody->Type == reactphysics3d::BodyType::DYNAMIC)
	{
		body->setMass(glm::max(Rigidbody->Mass, 0.001f));

		body->updateLocalInertiaTensorFromColliders();
		body->setLocalCenterOfMass(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
	}

	body->setLinearVelocity(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
	body->setAngularVelocity(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
}

void ColliderComponent::ToJson(json& j, const ColliderComponent& col)
{
	j["type"] = static_cast<int>(col.ColliderType);
	j["localPosition"] = { col.LocalPosition.x, col.LocalPosition.y, col.LocalPosition.z };
	j["localRotation"] = { col.LocalRotation.x, col.LocalRotation.y, col.LocalRotation.z, col.LocalRotation.w };

	if (col.ColliderType == Type::Box)
	{
		j["size"] = { col.Size.x, col.Size.y, col.Size.z };
	}
	else if (col.ColliderType == Type::Sphere)
	{
		j["radius"] = col.Radius;
	}
	else if (col.ColliderType == Type::Capsule)
	{
		j["radius"] = col.Radius;
		j["height"] = col.Height;
	}
}

void ColliderComponent::FromJson(const json& j, ColliderComponent& col, AssetManager* assetManager)
{
	col.ColliderType = static_cast<Type>(j.at("type"));

	if (j.contains("localPosition"))
	{
		auto pos = j.at("localPosition");
		col.LocalPosition = glm::vec3(pos[0], pos[1], pos[2]);
	}

	if (j.contains("localRotation"))
	{
		auto rot = j.at("localRotation");
		col.LocalRotation = glm::quat(rot[3], rot[0], rot[1], rot[2]);
	}

	if (col.ColliderType == Type::Box)
	{
		col.Size.x = j.at("size")[0];
		col.Size.y = j.at("size")[1];
		col.Size.z = j.at("size")[2];
	}
	else if (col.ColliderType == Type::Sphere)
	{
		col.Radius = j.at("radius");
	}
	else if (col.ColliderType == Type::Capsule)
	{
		col.Radius = j.at("radius");
		col.Height = j.at("height");
	}

	col.CreateCollider(col);
}
