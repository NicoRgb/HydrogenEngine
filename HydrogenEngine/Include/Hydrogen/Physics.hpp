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

		void UpdatePhysics(float timestep);
		void Update();

		const reactphysics3d::Array<reactphysics3d::DebugRenderer::DebugLine>& GetDebugLines() const { return m_PhysicsWorld->getDebugRenderer().getLines(); }
		const reactphysics3d::Array<reactphysics3d::DebugRenderer::DebugTriangle>& GetDebugTriangles() const { return m_PhysicsWorld->getDebugRenderer().getTriangles(); }

		static reactphysics3d::PhysicsCommon PhysicsCommon;

	private:
		class Scene* m_Scene;
		reactphysics3d::PhysicsWorld* m_PhysicsWorld;
	};

	struct RigidbodyComponent
	{
		RigidbodyComponent(class Entity entity);

		reactphysics3d::RigidBody* Rigidbody;

		void ApplyForce(glm::vec3 force)
		{
			Rigidbody->applyLocalForceAtCenterOfMass({force.x, force.y, force.z});
		}

		void ApplyTorque(glm::vec3 torque)
		{
			Rigidbody->applyLocalTorque({ torque.x, torque.y, torque.z });
		}

		static void OnImGuiRender(RigidbodyComponent& r)
		{
			if (ImGui::TreeNode("Rigidbody"))
			{
				const char* types[] = { "Static", "Kinematic", "Dynamic" };
				int currentIndex = 0;
				switch (r.Rigidbody->getType())
				{
				case reactphysics3d::BodyType::STATIC:
					currentIndex = 0;
					break;
				case reactphysics3d::BodyType::KINEMATIC:
					currentIndex = 1;
					break;
				case reactphysics3d::BodyType::DYNAMIC:
					currentIndex = 2;
					break;
				default:
					break;
				}

				if (ImGui::Combo("Type", &currentIndex, types, IM_ARRAYSIZE(types)))
				{
					reactphysics3d::BodyType bodyType = reactphysics3d::BodyType::STATIC;
					switch (currentIndex)
					{
					case 0:
						bodyType = reactphysics3d::BodyType::STATIC;
						break;
					case 1:
						bodyType = reactphysics3d::BodyType::KINEMATIC;
						break;
					case 2:
						bodyType = reactphysics3d::BodyType::DYNAMIC;
						break;
					default:
						break;
					}

					r.Rigidbody->setType(bodyType);
				}

				float mass = r.Rigidbody->getMass();
				if (ImGui::InputFloat("Mass", &mass))
				{
					r.Rigidbody->setMass(mass);
				}

				float linearDampening = r.Rigidbody->getLinearDamping();
				if (ImGui::InputFloat("Linear Dampening", &linearDampening))
				{
					r.Rigidbody->setLinearDamping(linearDampening);
				}

				float angularDampening = r.Rigidbody->getAngularDamping();
				if (ImGui::InputFloat("Angular Dampening", &angularDampening))
				{
					r.Rigidbody->setAngularDamping(angularDampening);
				}

				bool gravity = r.Rigidbody->isGravityEnabled();
				if (ImGui::Checkbox("Gravity", &gravity))
				{
					r.Rigidbody->enableGravity(gravity);
				}

				ImGui::TreePop();
			}
		}

		static void ToJson(json& j, const RigidbodyComponent& rb)
		{
			int typeIndex = 0;
			switch (rb.Rigidbody->getType())
			{
			case reactphysics3d::BodyType::STATIC:
				typeIndex = 0;
				break;
			case reactphysics3d::BodyType::KINEMATIC:
				typeIndex = 1;
				break;
			case reactphysics3d::BodyType::DYNAMIC:
				typeIndex = 2;
				break;
			default:
				break;
			}

			j = json{ { "type", typeIndex }, { "mass", rb.Rigidbody->getMass() },
				{ "linearDampening", rb.Rigidbody->getLinearDamping() }, { "angularDampening", rb.Rigidbody->getAngularDamping() },
				{ "gravity", rb.Rigidbody->isGravityEnabled() } };
		}

		static void FromJson(const json& j, RigidbodyComponent& rb, class AssetManager* assetManager)
		{
			int typeIndex = j.at("type");
			reactphysics3d::BodyType bodyType = reactphysics3d::BodyType::STATIC;

			switch (typeIndex)
			{
			case 0:
				bodyType = reactphysics3d::BodyType::STATIC;
				break;
			case 1:
				bodyType = reactphysics3d::BodyType::KINEMATIC;
				break;
			case 2:
				bodyType = reactphysics3d::BodyType::DYNAMIC;
				break;
			default:
				break;
			}

			rb.Rigidbody->setType(bodyType);
			rb.Rigidbody->setMass(j.at("mass"));
			rb.Rigidbody->setLinearDamping(j.at("linearDampening"));
			rb.Rigidbody->setAngularDamping(j.at("angularDampening"));
			rb.Rigidbody->enableGravity(j.at("gravity"));
		}
	};

	struct ColliderComponent
	{
		enum class Type
		{
			Box,
			Sphere
		};

		void CreateCollider(ColliderComponent& col);

		ColliderComponent(class Entity entity);

		RigidbodyComponent* Rigidbody;
		reactphysics3d::Collider* Collider = nullptr;
		Type ColliderType = Type::Box;

		glm::vec3 Size = glm::vec3(1.0f);
		float Radius = 1.0f;

		static void OnImGuiRender(ColliderComponent& col);
		static void ToJson(json& j, const ColliderComponent& col);
		static void FromJson(const json& j, ColliderComponent& col, AssetManager* assetManager);
	};
}
