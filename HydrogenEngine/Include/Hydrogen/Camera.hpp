#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Input.hpp"

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

		virtual void CalculateView(Entity entity)
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

		virtual void CalculateProj()
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

	class FreeCamera : public CameraComponent
	{
	public:
		FreeCamera()
		{
			Active = false;
			NearPlane = 0.1f;
			FarPlane = 1000.0f;
			FOV = 60.0f;

			m_CameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
			m_CameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
			m_CameraUp = glm::vec3(0.0f, 1.0f, 0.0f);
			m_Yaw = -90.0f;
		}

		void Update(float dt)
		{
			if (!Input::IsMouseButtonDown(KeyCode::MouseRight))
			{
				return;
			}

			float cameraSpeed = 2.5f * dt;
			const float sensitivity = 0.1f;

			if (Input::IsKeyDown(KeyCode::W))
			{
				m_CameraPos += cameraSpeed * m_CameraFront;
			}
			if (Input::IsKeyDown(KeyCode::S))
			{
				m_CameraPos -= cameraSpeed * m_CameraFront;
			}
			if (Input::IsKeyDown(KeyCode::A))
			{
				m_CameraPos -= glm::normalize(glm::cross(m_CameraFront, m_CameraUp)) * cameraSpeed;
			}
			if (Input::IsKeyDown(KeyCode::D))
			{
				m_CameraPos += glm::normalize(glm::cross(m_CameraFront, m_CameraUp)) * cameraSpeed;
			}

			m_Yaw -= Input::GetMouseDeltaX() * sensitivity;
			m_Pitch += Input::GetMouseDeltaY() * sensitivity;

			if (m_Pitch > 89.0f)
			{
				m_Pitch = 89.0f;
			}
			if (m_Pitch < -89.0f)
			{
				m_Pitch = -89.0f;
			}
		}

		void CalculateView()
		{
			glm::vec3 direction;
			direction.x = cos(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
			direction.y = sin(glm::radians(m_Pitch));
			direction.z = sin(glm::radians(m_Yaw)) * cos(glm::radians(m_Pitch));
			m_CameraFront = glm::normalize(direction);

			View = glm::lookAt(m_CameraPos, m_CameraPos + m_CameraFront, m_CameraUp);
		}

	private:
		glm::vec3 m_CameraPos;
		glm::vec3 m_CameraFront;
		glm::vec3 m_CameraUp;

		float m_Yaw;
		float m_Pitch;
	};
}
