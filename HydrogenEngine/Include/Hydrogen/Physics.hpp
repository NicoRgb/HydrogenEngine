#pragma once

#include "Hydrogen/Scene.hpp"
#include <reactphysics3d/reactphysics3d.h>
#include <imgui.h>
#include <glm/glm.hpp>

namespace Hydrogen
{
	class PhysicsWorld
	{
	public:
		PhysicsWorld(const std::shared_ptr<Scene>& scene, glm::vec3 gravity);
		~PhysicsWorld();

		reactphysics3d::RigidBody* CreateRigidbody(const TransformComponent& transform);

		void Update(double timestep);

	private:
		std::shared_ptr<Scene> m_Scene;
		reactphysics3d::PhysicsWorld* m_PhysicsWorld;

		static reactphysics3d::PhysicsCommon s_PhysicsCommon;
	};

	struct RigidbodyComponent
	{
		reactphysics3d::RigidBody* Rigidbody;

		static void OnImGuiRender(RigidbodyComponent& t)
		{
			if (ImGui::TreeNode("Rigidbody"))
			{
				ImGui::TreePop();
			}
		}
	};
}
