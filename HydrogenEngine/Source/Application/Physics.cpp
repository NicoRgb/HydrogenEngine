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
	m_PhysicsWorld->setIsDebugRenderingEnabled(true);

	reactphysics3d::DebugRenderer& debug = m_PhysicsWorld->getDebugRenderer();
	debug.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLISION_SHAPE, true);
	debug.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::COLLIDER_AABB, true);
	debug.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::CONTACT_POINT, true);
	debug.setIsDebugItemDisplayed(reactphysics3d::DebugRenderer::DebugItem::CONTACT_NORMAL, true);
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

void PhysicsWorld::UpdatePhysics(float timestep)
{
	m_PhysicsWorld->update((reactphysics3d::decimal)timestep);
}

void PhysicsWorld::Update()
{
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

void ColliderComponent::CreateCollider(ColliderComponent& col)
{
	if (Rigidbody == nullptr)
	{
		return;
	}
	auto body = Rigidbody->Rigidbody;

	if (col.Collider)
	{
		body->removeCollider(col.Collider);
		col.Collider = nullptr;
	}

	reactphysics3d::Collider* newCollider = nullptr;

	if (col.ColliderType == ColliderComponent::Type::Box)
	{
		reactphysics3d::Vector3 halfExtents(col.Size.x * 0.5f, col.Size.y * 0.5f, col.Size.z * 0.5f);
		reactphysics3d::BoxShape* boxShape = PhysicsWorld::PhysicsCommon.createBoxShape(halfExtents);
		newCollider = body->addCollider(boxShape, reactphysics3d::Transform::identity());
	}
	else if (col.ColliderType == ColliderComponent::Type::Sphere)
	{
		reactphysics3d::SphereShape* sphereShape = PhysicsWorld::PhysicsCommon.createSphereShape(col.Radius);
		newCollider = body->addCollider(sphereShape, reactphysics3d::Transform::identity());
	}

	col.Collider = newCollider;
}

ColliderComponent::ColliderComponent(Entity entity)
	: Rigidbody(entity.TryGetComponent<RigidbodyComponent>()), Collider(nullptr)
{
	CreateCollider(*this);
}

void ColliderComponent::OnImGuiRender(ColliderComponent& col)
{
	if (ImGui::TreeNode("Collider"))
	{
		const char* types[] = { "Box", "Sphere" };
		int currentIndex = static_cast<int>(col.ColliderType);

		if (ImGui::Combo("Type", &currentIndex, types, IM_ARRAYSIZE(types)))
		{
			col.ColliderType = static_cast<Type>(currentIndex);

			col.CreateCollider(col);
		}

		if (col.ColliderType == Type::Box)
		{
			if (ImGui::DragFloat3("Size", glm::value_ptr(col.Size), 0.1f))
			{
				col.CreateCollider(col);
			}
		}
		else if (col.ColliderType == Type::Sphere)
		{
			if (ImGui::DragFloat("Radius", &col.Radius, 0.1f))
			{
				col.CreateCollider(col);
			}
		}

		ImGui::TreePop();
	}
}

void ColliderComponent::ToJson(json& j, const ColliderComponent& col)
{
	j["type"] = static_cast<int>(col.ColliderType);
	if (col.ColliderType == Type::Box)
	{
		j["size"] = { col.Size.x, col.Size.y, col.Size.z };
	}
	else
	{
		j["radius"] = col.Radius;
	}
}

void ColliderComponent::FromJson(const json& j, ColliderComponent& col, AssetManager* assetManager)
{
	col.ColliderType = static_cast<Type>(j.at("type"));
	if (col.ColliderType == Type::Box)
	{
		col.Size.x = j.at("size")[0];
		col.Size.y = j.at("size")[1];
		col.Size.z = j.at("size")[2];
	}
	else
	{
		col.Radius = j.at("radius");
	}

	col.CreateCollider(col);
}
