#pragma once

#include "ReactPhysicsWrapper.hpp"
#include <imgui.h>
#include <json.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace nlohmann;

namespace Hydrogen
{
	class PhysicsWorld
	{
	public:
		PhysicsWorld(class Scene* scene, glm::vec3 gravity);
		PhysicsWorld() = default;
		~PhysicsWorld();

		void Reset();

		reactphysics3d::PhysicsWorld* GetPhysicsWorld() const { return m_PhysicsWorld; }

		reactphysics3d::RigidBody* CreateRigidbody(const struct TransformComponent& transform) const;
		void DestroyRigidbody(reactphysics3d::RigidBody* body);

		void UpdatePhysics(float timestep);
		void SyncTransforms();

		static reactphysics3d::PhysicsCommon PhysicsCommon;

	private:
		class Scene* m_Scene = nullptr;
		reactphysics3d::PhysicsWorld* m_PhysicsWorld = nullptr;
	};

	struct RigidbodyComponent
	{
		reactphysics3d::RigidBody* Rigidbody = nullptr;

		reactphysics3d::BodyType Type = reactphysics3d::BodyType::STATIC;
		float Mass = 1.0f;
		float LinearDamping = 0.0f;
		float AngularDamping = 0.0f;
		bool UseGravity = true;

		bool LockLinearX = false;
		bool LockLinearY = false;
		bool LockLinearZ = false;

		bool LockAngularX = false;
		bool LockAngularY = false;
		bool LockAngularZ = false;

		void ApplyRotationLock();

		RigidbodyComponent() = default;
		RigidbodyComponent(class Entity entity);

		void SetType(reactphysics3d::BodyType type);
		void SetMass(float mass);
		void SetUseGravity(bool useGravity);

		void ApplyForce(glm::vec3 force);
		void ApplyTorque(glm::vec3 torque);
		void SetLinearVelocity(glm::vec3 velocity);
		void SetAngularVelocity(glm::vec3 velocity);

		static void ToJson(json& j, const RigidbodyComponent& rb);
		static void FromJson(const json& j, RigidbodyComponent& rb, class AssetManager* assetManager);
	};

	struct ColliderComponent
	{
		enum class Type
		{
			Box,
			Sphere,
			Capsule
		};

		RigidbodyComponent* Rigidbody = nullptr;
		reactphysics3d::Collider* Collider = nullptr;
		Type ColliderType = Type::Box;

		glm::vec3 Size = glm::vec3(1.0f);
		float Radius = 0.5f;
		float Height = 1.0f;

		glm::vec3 LocalPosition = glm::vec3(0.0f);
		glm::quat LocalRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		ColliderComponent() = default;
		ColliderComponent(class Entity entity);

		void DestroyCollider();
		void CreateCollider(ColliderComponent& col);

		static void ToJson(json& j, const ColliderComponent& col);
		static void FromJson(const json& j, ColliderComponent& col, AssetManager* assetManager);
	};
}
