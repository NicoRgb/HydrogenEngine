#pragma once

#include "Hydrogen/ReactPhysicsWrapper.hpp"
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
}
