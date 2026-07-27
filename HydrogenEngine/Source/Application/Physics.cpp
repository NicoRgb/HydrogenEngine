#include "Hydrogen/Physics.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Application.hpp"
#include <glm/glm.hpp>

using namespace Hydrogen;

reactphysics3d::PhysicsCommon PhysicsWorld::PhysicsCommon;

PhysicsWorld::PhysicsWorld(Scene* scene, glm::vec3 gravity)
	: m_Scene(scene), m_PhysicsWorld(PhysicsCommon.createPhysicsWorld())
{
	m_PhysicsWorld->setGravity({ gravity.x, gravity.y, gravity.z });
}

PhysicsWorld::~PhysicsWorld()
{
}

reactphysics3d::RigidBody* PhysicsWorld::CreateRigidbody(const TransformComponent& transform) const
{
	HY_ASSERT(m_PhysicsWorld, "Physics world is null!");

	glm::vec3 translation, scale;
	glm::quat rotation;
	TransformComponent::DecomposeTransform(transform.Transform, translation, rotation, scale);
	glm::quat normRot = glm::normalize(rotation);

	reactphysics3d::Vector3 position(translation.x, translation.y, translation.z);
	reactphysics3d::Quaternion orientation(normRot.x, normRot.y, normRot.z, normRot.w);
	reactphysics3d::Transform t(position, orientation);

	auto body = m_PhysicsWorld->createRigidBody(t);
	HY_ASSERT(body, "Failed to create rigidbody!");

	body->setType(reactphysics3d::BodyType::STATIC);

	return body;
}

void PhysicsWorld::UpdatePhysics(float timestep)
{
	if (m_PhysicsWorld)
	{
		m_PhysicsWorld->update((reactphysics3d::decimal)timestep);
	}
}

void PhysicsWorld::Update()
{
	if (!m_PhysicsWorld || !m_Scene) return;

	m_Scene->IterateComponents<TransformComponent, RigidbodyComponent>([](Entity entity, TransformComponent& transform, RigidbodyComponent& rb)
		{
			if (!rb.Rigidbody) return;

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

RigidbodyComponent::RigidbodyComponent(Entity entity)
{
	Rigidbody = Application::Get()->CurrentScene->GetScene()->GetPhysicsWorld().CreateRigidbody(entity.GetComponent<TransformComponent>());
}

ColliderComponent::ColliderComponent(Entity entity)
	: Rigidbody(entity.TryGetComponent<RigidbodyComponent>()), Collider(nullptr)
{
	CreateCollider(*this);
}

void ColliderComponent::CreateCollider(ColliderComponent& col)
{
	if (Rigidbody == nullptr)
	{
		return;
	}
	auto body = Rigidbody->Rigidbody;
	if (!body)
	{
		HY_ENGINE_WARN("Cannot create collider: rigidbody pointer is invalid!");
		return;
	}

	if (col.Collider)
	{
		body->removeCollider(col.Collider);
		col.Collider = nullptr;
	}

	glm::quat normRot = glm::normalize(col.LocalRotation);

	reactphysics3d::Vector3 position(col.LocalPosition.x, col.LocalPosition.y, col.LocalPosition.z);
	reactphysics3d::Quaternion orientation(normRot.x, normRot.y, normRot.z, normRot.w);
	reactphysics3d::Transform localTransform(position, orientation);

	reactphysics3d::Collider* newCollider = nullptr;

	if (col.ColliderType == ColliderComponent::Type::Box)
	{
		reactphysics3d::Vector3 halfExtents(col.Size.x * 0.5f, col.Size.y * 0.5f, col.Size.z * 0.5f);
		reactphysics3d::BoxShape* boxShape = PhysicsWorld::PhysicsCommon.createBoxShape(halfExtents);
		newCollider = body->addCollider(boxShape, localTransform);
	}
	else if (col.ColliderType == ColliderComponent::Type::Sphere)
	{
		reactphysics3d::SphereShape* sphereShape = PhysicsWorld::PhysicsCommon.createSphereShape(col.Radius);
		newCollider = body->addCollider(sphereShape, localTransform);
	}
	else if (col.ColliderType == ColliderComponent::Type::Capsule)
	{
		reactphysics3d::CapsuleShape* capsuleShape = PhysicsWorld::PhysicsCommon.createCapsuleShape(col.Radius, col.Height);
		newCollider = body->addCollider(capsuleShape, localTransform);
	}

	col.Collider = newCollider;

	body->updateLocalInertiaTensorFromColliders();
	body->setLocalCenterOfMass(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
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
