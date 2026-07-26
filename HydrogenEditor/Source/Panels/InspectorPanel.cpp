#include <imgui.h>
#include "Panels/InspectorPanel.hpp"
#include "Hydrogen/Animation.hpp"

using namespace Hydrogen;

template<typename T>
void DrawComponentUI(T& component);

template<typename T>
const char* GetComponentName()
{
	if constexpr (std::is_same_v<T, SkeletalMeshRendererComponent>) return "Skeletal Mesh Renderer";
	else if constexpr (std::is_same_v<T, AnimatorComponent>) return "Animator";
	else if constexpr (std::is_same_v<T, MeshRendererComponent>) return "Mesh Renderer";
	else if constexpr (std::is_same_v<T, DirectionalLightComponent>) return "Directional Light";
	else if constexpr (std::is_same_v<T, PointLightComponent>) return "Point Light";
	else if constexpr (std::is_same_v<T, RigidbodyComponent>) return "Rigidbody";
	else if constexpr (std::is_same_v<T, ColliderComponent>) return "Collider";
	else if constexpr (std::is_same_v<T, CameraComponent>) return "Camera";
	else if constexpr (std::is_same_v<T, ScriptComponent>) return "Script";
	else return "Unknown Component";
}

template<typename T>
static void DrawComponent(Entity entity)
{
	if (!entity.HasComponent<T>())
	{
		return;
	}

	if constexpr (!std::is_same_v<T, TagComponent> && !std::is_same_v<T, TransformComponent>)
	{
		std::stringstream sstream;
		sstream << "X###Remove_";
		sstream << GetComponentName<T>();

		if (ImGui::Button(sstream.str().c_str()))
		{
			entity.RemoveComponent<T>();
			return;
		}

		ImGui::SameLine();
	}
	
	T& component = entity.GetComponent<T>();
	DrawComponentUI<T>(component);
}

template<typename... Ts>
static void DrawAllComponents(Entity entity)
{
	(DrawComponent<Ts>(entity), ...);
}

template<typename... Ts>
void DrawAddComponentMenu(Scene* scene, Entity entity)
{
	if (ImGui::Button("Add Component"))
	{
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (ImGui::BeginPopup("AddComponentPopup"))
	{
		(..., [&] {
			using T = Ts;
			if (!entity.HasComponent<T>())
			{
				if (ImGui::MenuItem(GetComponentName<T>()))
				{
					entity.AddComponent<T>();
					ImGui::CloseCurrentPopup();
				}
			}
			}());

		ImGui::EndPopup();
	}
}

void InspectorPanel::OnAttach()
{
	Dockspace->GetEventBus().Subscribe<SceneChangeEvent>([this](const SceneChangeEvent& e) {
		m_Scene = e.Scene;
		});

	Dockspace->GetEventBus().Subscribe<EntitySelectedEvent>([this](const EntitySelectedEvent& e) {
		m_SelectedEntity = e.SelectedEntity;
		});
}

void InspectorPanel::OnImGuiRender()
{
	if (m_SelectedEntity.IsValid())
	{
		if (ImGui::Button("Remove"))
		{
			auto e = m_SelectedEntity;
			m_SelectedEntity = Entity();
			e.Delete();
			return;
		}

		ImGui::Separator();
		DrawAllComponents<TagComponent, TransformComponent, SkeletalMeshRendererComponent, AnimatorComponent, MeshRendererComponent, DirectionalLightComponent, PointLightComponent, RigidbodyComponent, ColliderComponent, CameraComponent, ScriptComponent>
			(m_SelectedEntity);
		ImGui::Separator();
		DrawAddComponentMenu<SkeletalMeshRendererComponent, AnimatorComponent, MeshRendererComponent, DirectionalLightComponent, PointLightComponent, RigidbodyComponent, ColliderComponent, CameraComponent, ScriptComponent>
			(m_Scene, m_SelectedEntity);
	}
	else
	{
		ImGui::Text("No entity selected");
	}
}

template<>
inline void DrawComponentUI<TagComponent>(TagComponent& comp)
{
	char buffer[256];
	memset(buffer, 0, sizeof(buffer));
	std::strncpy(buffer, comp.Name.c_str(), sizeof(buffer) - 1);

	if (ImGui::InputText("Tag", buffer, sizeof(buffer)))
	{
		comp.Name = std::string(buffer);
	}
}

template<>
inline void DrawComponentUI<TransformComponent>(TransformComponent& comp)
{
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen;
	if (ImGui::TreeNodeEx("Transform", flags))
	{
		glm::vec3 translation, rotation, scale;
		comp.DecomposeTransform(comp.Transform, translation, rotation, scale);

		bool changed = false;

		changed |= ImGui::DragFloat3("Translation", glm::value_ptr(translation), 0.1f);
		changed |= ImGui::DragFloat3("Rotation", glm::value_ptr(rotation), 0.1f);
		changed |= ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.1f);

		if (changed)
			comp.Transform = comp.RecomposeTransform(translation, rotation, scale);

		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<MeshRendererComponent>(MeshRendererComponent& comp)
{
	if (ImGui::TreeNode("Mesh Renderer"))
	{
		AssetPicker("Static Mesh", comp.Mesh);
		AssetPicker("Material", comp.Material);

		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<DirectionalLightComponent>(DirectionalLightComponent& comp)
{
	if (ImGui::TreeNode("DirectionalLight"))
	{
		ImGui::ColorPicker3("Color", glm::value_ptr(comp.Color));
		ImGui::SliderFloat("Intensity", &comp.Intensity, 0.0f, 10.0f);

		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<PointLightComponent>(PointLightComponent& comp)
{
	if (ImGui::TreeNode("PointLight"))
	{
		ImGui::ColorPicker3("Color", glm::value_ptr(comp.Color));

		ImGui::SliderFloat("Intensity", &comp.Intensity, 0.0f, 10.0f);
		ImGui::SliderFloat("Radius", &comp.Radius, 0.1f, 100.0f);

		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<ScriptComponent>(ScriptComponent& comp)
{
	if (ImGui::TreeNode("Script"))
	{
		AssetPicker("Script", comp.Script);
		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<RigidbodyComponent>(RigidbodyComponent& comp)
{
	if (ImGui::TreeNode("Rigidbody"))
	{
		const char* types[] = { "Static", "Kinematic", "Dynamic" };
		int currentIndex = 0;
		switch (comp.Rigidbody->getType())
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

			comp.Rigidbody->setType(bodyType);
		}

		float mass = comp.Rigidbody->getMass();
		if (ImGui::InputFloat("Mass", &mass))
		{
			comp.Rigidbody->setMass(mass);
		}

		float linearDampening = comp.Rigidbody->getLinearDamping();
		if (ImGui::InputFloat("Linear Dampening", &linearDampening))
		{
			comp.Rigidbody->setLinearDamping(linearDampening);
		}

		float angularDampening = comp.Rigidbody->getAngularDamping();
		if (ImGui::InputFloat("Angular Dampening", &angularDampening))
		{
			comp.Rigidbody->setAngularDamping(angularDampening);
		}

		bool gravity = comp.Rigidbody->isGravityEnabled();
		if (ImGui::Checkbox("Gravity", &gravity))
		{
			comp.Rigidbody->enableGravity(gravity);
		}

		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<ColliderComponent>(ColliderComponent& comp)
{
	if (ImGui::TreeNode("Collider"))
	{
		const char* types[] = { "Box", "Sphere" };
		int currentIndex = static_cast<int>(comp.ColliderType);

		if (ImGui::Combo("Type", &currentIndex, types, IM_ARRAYSIZE(types)))
		{
			comp.ColliderType = static_cast<ColliderComponent::Type>(currentIndex);

			comp.CreateCollider(comp);
		}

		if (comp.ColliderType == ColliderComponent::Type::Box)
		{
			if (ImGui::DragFloat3("Size", glm::value_ptr(comp.Size), 0.1f))
			{
				comp.CreateCollider(comp);
			}
		}
		else if (comp.ColliderType == ColliderComponent::Type::Sphere)
		{
			if (ImGui::DragFloat("Radius", &comp.Radius, 0.1f))
			{
				comp.CreateCollider(comp);
			}
		}

		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<SkeletalMeshRendererComponent>(SkeletalMeshRendererComponent& comp)
{
	if (ImGui::TreeNode("Skeletal Mesh Renderer"))
	{
		AssetPicker("Skeleton", comp.Skeleton);
		AssetPicker("Skeletal Mesh", comp.SkeletalMesh);
		AssetPicker("Material", comp.Material);

		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<AnimatorComponent>(AnimatorComponent& comp)
{
	if (ImGui::TreeNode("Animator"))
	{
		if (AssetPicker("Animation Graph", comp.AnimationGraph))
			comp.UpdateGraph();

		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<CameraComponent>(CameraComponent& comp)
{
	if (ImGui::TreeNode("Camera"))
	{
		ImGui::Checkbox("Active", &comp.Active);

		ImGui::DragFloat("Near Plane", &comp.NearPlane, 0.1f);
		ImGui::DragFloat("Far Plane", &comp.FarPlane, 0.1f);
		ImGui::DragFloat("FOV", &comp.FOV, 0.1f);

		ImGui::TreePop();
	}
}
