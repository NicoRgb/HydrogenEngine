#pragma once

#include <glm/glm.hpp>

#include "Hydrogen/Scene.hpp"

namespace Hydrogen
{
	struct CameraComponent
	{
		CameraComponent()
		{
		}

		CameraComponent(Entity entity)
		{
			(void)entity;
			Active = false;
			NearPlane = 0.1f;
			FarPlane = 1000.0f;
			FOV = 60.0f;
		}

		glm::mat4 View, Proj;
		uint32_t ViewportWidth, ViewportHeight;

		bool Active;
		float NearPlane, FarPlane;
		float FOV;

		void CalculateView(Entity entity)
		{
			glm::mat4& transform = entity.GetComponent<TransformComponent>().Transform;

			glm::vec3 position, rotation, scale;
			TransformComponent::DecomposeTransform(transform, position, rotation, scale);
			
			glm::vec3 front;
			front.x = cos(rotation.y) * cos(rotation.x);
			front.y = sin(rotation.x);
			front.z = sin(rotation.y) * cos(rotation.x);
			front = glm::normalize(front);

			glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));
			glm::vec3 up = glm::normalize(glm::cross(right, front));

			View = glm::lookAt(
				position,
				position + front,
				up
			);
		}

		void CalculateProj(Entity entity)
		{
			Proj = glm::perspective(
				glm::radians(FOV),
				(float)ViewportWidth / (float)ViewportHeight,
				NearPlane, FarPlane
			);

			Proj[1][1] *= -1;
		}

		static void OnImGuiRender(CameraComponent& c)
		{
			if (ImGui::TreeNode("Camera"))
			{
				ImGui::Checkbox("Active", &c.Active);

				ImGui::DragFloat("Near Plane", &c.NearPlane, 0.1f);
				ImGui::DragFloat("Far Plane", &c.FarPlane, 0.1f);
				ImGui::DragFloat("FOV", &c.FOV, 0.1f);

				ImGui::TreePop();
			}
		}

		static void ToJson(json& j, const CameraComponent& c)
		{
			j["Active"] = c.Active;
			j["NearPlane"] = c.NearPlane;
			j["FarPlane"] = c.FarPlane;
			j["FOV"] = c.FOV;
		}

		static void FromJson(const json& j, CameraComponent& c, AssetManager* assetManager)
		{
			c.Active = j.at("Active");
			c.NearPlane = j.at("NearPlane");
			c.FarPlane = j.at("FarPlane");
			c.FOV = j.at("FOV");
		}
	};
}
