#pragma once

#include <reactphysics3d/reactphysics3d.h>
#include <imgui.h>
#include <glm/glm.hpp>
#include <json.hpp>

using namespace nlohmann;

namespace Hydrogen
{
	class PhysicsWorld
	{
	public:
		PhysicsWorld(class Scene* scene, glm::vec3 gravity);
		PhysicsWorld() = default;
		~PhysicsWorld();

		reactphysics3d::RigidBody* CreateRigidbody(const struct TransformComponent& transform) const;

		void Update(float timestep);

	private:
		class Scene* m_Scene;
		reactphysics3d::PhysicsWorld* m_PhysicsWorld;

		static reactphysics3d::PhysicsCommon s_PhysicsCommon;
	};

	struct RigidbodyComponent
	{
		RigidbodyComponent(class Entity entity);

		reactphysics3d::RigidBody* Rigidbody;

		static void OnImGuiRender(RigidbodyComponent& t)
		{
			if (ImGui::TreeNode("Rigidbody"))
			{
				ImGui::TreePop();
			}
		}

		static void ToJson(json& j, const RigidbodyComponent& rb)
		{
		}

		static void FromJson(const json& j, RigidbodyComponent& rb, class AssetManager* assetManager)
		{
		}
	};
}
