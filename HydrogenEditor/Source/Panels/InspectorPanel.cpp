#include <imgui.h>
#include "Panels/InspectorPanel.hpp"
#include "Hydrogen/Scene/Animation.hpp"

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
	else if constexpr (std::is_same_v<T, ScriptsComponent>) return "Script";
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
	m_SelectedEntityUUID = 0;

	Dockspace->GetEventBus().Subscribe<SceneChangeEvent>([this](const SceneChangeEvent& e) {
		m_Scene = e.Scene;
		});

	Dockspace->GetEventBus().Subscribe<EntitySelectedEvent>([this](const EntitySelectedEvent& e) {
		m_SelectedEntityUUID = e.SelectedEntityUUID;
		});
}

void InspectorPanel::OnImGuiRender()
{
	auto selectedEntity = m_Scene->GetEntityByUUID(m_SelectedEntityUUID);
	if (selectedEntity.IsValid())
	{
		if (ImGui::Button("Remove"))
		{
			Dockspace->GetEventBus().Publish<EntitySelectedEvent>({ 0 });
			selectedEntity.Delete();
			return;
		}

		ImGui::Separator();
		DrawAllComponents<TagComponent, TransformComponent, SkeletalMeshRendererComponent, AnimatorComponent, MeshRendererComponent, DirectionalLightComponent, PointLightComponent, RigidbodyComponent, ColliderComponent, CameraComponent, ScriptsComponent>
			(selectedEntity);
		ImGui::Separator();
		DrawAddComponentMenu<SkeletalMeshRendererComponent, AnimatorComponent, MeshRendererComponent, DirectionalLightComponent, PointLightComponent, RigidbodyComponent, ColliderComponent, CameraComponent, ScriptsComponent>
			(m_Scene, selectedEntity);
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
		glm::vec3 translation = comp.GetTranslation();
		glm::vec3 scale = comp.GetScale();
		glm::quat rotation = comp.GetRotation();

		if (ImGui::DragFloat3("Translation", glm::value_ptr(translation), 0.1f))
			comp.SetTranslation(translation);

		if (ImGui::DragFloat4("Rotation", glm::value_ptr(rotation), 0.1f))
			comp.SetRotation(rotation);

		if (ImGui::DragFloat3("Scale", glm::value_ptr(scale), 0.1f))
			comp.SetScale(scale);

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
inline void DrawComponentUI<ScriptsComponent>(ScriptsComponent & comp)
{
	if (ImGui::Button("+ Add Script"))
	{
		comp.Scripts.push_back(std::make_unique<ScriptDesc>());
	}

	ImGui::Separator();

	int scriptToRemove = -1;

	for (size_t i = 0; i < comp.Scripts.size(); ++i)
	{
		auto& script = comp.Scripts[i];

		std::string scriptName = script->Script
			? std::filesystem::path(script->Script->GetPath()).stem().string()
			: "[Empty Script]";

		ImGui::PushID(static_cast<int>(i));

		if (ImGui::Button("X"))
		{
			scriptToRemove = static_cast<int>(i);
		}
		ImGui::SameLine();

		bool nodeOpen = ImGui::TreeNode((scriptName + "##" + std::to_string(i)).c_str());

		if (ImGui::BeginPopupContextItem("ScriptContext"))
		{
			if (ImGui::MenuItem("Remove Script"))
			{
				scriptToRemove = static_cast<int>(i);
			}
			ImGui::EndPopup();
		}

		if (nodeOpen)
		{
			AssetPicker("Script", script->Script);

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	if (scriptToRemove != -1)
	{
		comp.Scripts.erase(comp.Scripts.begin() + scriptToRemove);
	}
}

template<>inline void DrawComponentUI<RigidbodyComponent>(RigidbodyComponent& comp)
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
			comp.Type = (int)bodyType;
		}

		float mass = comp.Rigidbody->getMass();
		if (ImGui::InputFloat("Mass", &mass))
		{
			comp.Mass = mass;
			comp.Rigidbody->setMass(mass);
		}

		float linearDampening = comp.Rigidbody->getLinearDamping();
		if (ImGui::InputFloat("Linear Dampening", &linearDampening))
		{
			comp.LinearDamping = linearDampening;
			comp.Rigidbody->setLinearDamping(linearDampening);
		}

		float angularDampening = comp.Rigidbody->getAngularDamping();
		if (ImGui::InputFloat("Angular Dampening", &angularDampening))
		{
			comp.AngularDamping = angularDampening;
			comp.Rigidbody->setAngularDamping(angularDampening);
		}

		bool gravity = comp.Rigidbody->isGravityEnabled();
		if (ImGui::Checkbox("Gravity", &gravity))
		{
			comp.UseGravity = gravity;
			comp.Rigidbody->enableGravity(gravity);
		}

		ImGui::Separator();
		ImGui::Text("Constraints");

		if (ImGui::Checkbox("Lock Linear X", &comp.LockLinearX) ||
			ImGui::Checkbox("Lock Linear Y", &comp.LockLinearY) ||
			ImGui::Checkbox("Lock Linear Z", &comp.LockLinearZ))
		{
			comp.ApplyRotationLock();
		}

		if (ImGui::Checkbox("Lock Rotation X", &comp.LockAngularX) ||
			ImGui::Checkbox("Lock Rotation Y", &comp.LockAngularY) ||
			ImGui::Checkbox("Lock Rotation Z", &comp.LockAngularZ))
		{
			comp.ApplyRotationLock();
		}

		ImGui::TreePop();
	}
}

template<>
inline void DrawComponentUI<ColliderComponent>(ColliderComponent& comp)
{
	if (ImGui::TreeNode("Collider"))
	{
		const char* types[] = { "Box", "Sphere", "Capsule" };
		int currentIndex = comp.ColliderType;

		if (ImGui::Combo("Type##collider", &currentIndex, types, IM_ARRAYSIZE(types)))
		{
			comp.ColliderType = (currentIndex);
			comp.CreateCollider();
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Local Transform");

		if (ImGui::DragFloat3("Local Position##collider", glm::value_ptr(comp.LocalPosition), 0.01f))
		{
			comp.CreateCollider();
		}

		glm::vec3 eulerAngles = glm::eulerAngles(comp.LocalRotation);
		glm::vec3 eulerDegrees = glm::degrees(eulerAngles);

		if (ImGui::DragFloat3("Local Rotation##collider", glm::value_ptr(eulerDegrees), 1.0f, -180.0f, 180.0f))
		{
			comp.LocalRotation = glm::quat(glm::radians(eulerDegrees));
			comp.CreateCollider();
		}

		ImGui::Spacing();
		ImGui::SeparatorText("Shape Properties");

		if ((ColliderComponent::Type)comp.ColliderType == ColliderComponent::Type::Box)
		{
			if (ImGui::DragFloat3("Box Size##collider", glm::value_ptr(comp.Size), 0.05f, 0.1f, 100.0f))
			{
				comp.CreateCollider();
			}
		}
		else if ((ColliderComponent::Type)comp.ColliderType == ColliderComponent::Type::Sphere)
		{
			if (ImGui::DragFloat("Sphere Radius##collider", &comp.Radius, 0.05f, 0.1f, 50.0f))
			{
				comp.CreateCollider();
			}
		}
		else if ((ColliderComponent::Type)comp.ColliderType == ColliderComponent::Type::Capsule)
		{
			if (ImGui::DragFloat("Capsule Radius##collider", &comp.Radius, 0.05f, 0.1f, 50.0f))
			{
				comp.CreateCollider();
			}
			if (ImGui::DragFloat("Capsule Height##collider", &comp.Height, 0.05f, 0.1f, 100.0f))
			{
				comp.CreateCollider();
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
