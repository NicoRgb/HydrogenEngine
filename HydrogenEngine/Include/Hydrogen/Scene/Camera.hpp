#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "Hydrogen/Scene/Components.hpp"
#include "Hydrogen/Input.hpp"

namespace Hydrogen
{
	struct CameraComponent : GenericComponent
	{
		CameraComponent(Entity entity)
			: GenericComponent(entity)
		{
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
			const auto& transform = entity.GetComponent<TransformComponent>();

			glm::vec3 translation = transform.GetTranslation();
			glm::quat rotation = transform.GetRotation();

			glm::vec3 front = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
			glm::vec3 up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);

			View = glm::lookAt(
				translation,
				translation + front,
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

		BEGIN_COMPONENT_REFLECTION(CameraComponent)
			REFLECT_MEMBER(Active)
			REFLECT_MEMBER(NearPlane)
			REFLECT_MEMBER(FarPlane)
			REFLECT_MEMBER(FOV)
		END_COMPONENT_REFLECTION()
	};
	REGISTER_COMPONENT(CameraComponent, "CameraComponent")

	class FreeCamera : public CameraComponent
	{
	public:
		FreeCamera()
			: CameraComponent(Entity())
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
			float cameraSpeed = 2.5f * dt;
			const float sensitivity = 0.5f;

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

		glm::vec3 GetPosition() const { return m_CameraPos; }
		void SetPosition(glm::vec3 position) { m_CameraPos = position; }

		virtual void Serialize(json&) const override {}
		virtual void Deserialize(const json&) override {}

	private:
		glm::vec3 m_CameraPos;
		glm::vec3 m_CameraFront;
		glm::vec3 m_CameraUp;

		float m_Yaw;
		float m_Pitch;
	};
}
