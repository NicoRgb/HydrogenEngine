#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>

#include <glm/gtx/matrix_decompose.hpp>

void ImGuiTransform(glm::mat4& transform, const char* label = "Transform")
{
    ImGui::PushID(label);

    glm::vec3 translation, scale, skew;
    glm::vec4 perspective;
    glm::quat rotation;
    glm::decompose(transform, scale, rotation, translation, skew, perspective);

    glm::vec3 euler = glm::degrees(glm::eulerAngles(rotation));

    bool changed = false;

    ImGui::TextUnformatted(label);
    changed |= ImGui::DragFloat3("Translation", &translation.x, 0.1f);
    changed |= ImGui::DragFloat3("Rotation", &euler.x, 0.5f);
    changed |= ImGui::DragFloat3("Scale", &scale.x, 0.01f);

    if (changed) {
        rotation = glm::quat(glm::radians(euler));
        transform = glm::translate(glm::mat4(1.0f), translation)
                  * glm::mat4_cast(rotation)
                  * glm::scale(glm::mat4(1.0f), scale);
    }

    ImGui::PopID();
}

class EditorApp : public Hydrogen::Application
{
public:
	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Editor";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Editor";
		ApplicationSpec.ViewportSize = { 1920, 1080 };
		ApplicationSpec.UseDebugGUI = true;
	}

	virtual void OnStartup() override
	{
	}

	virtual void OnShutdown() override
	{
	}

	virtual void OnUpdate() override
	{
	}

	virtual void OnImGuiRender() override
	{
		ImGui::Begin("Scene");
		ImGui::Text("Current Scene");
		int i = 0;
		CurrentScene->IterateComponents<Hydrogen::TransformComponent>([&](Hydrogen::TransformComponent& transform)
			{
				char c = (char)((int)'0' + i++);
				ImGuiTransform(transform.Transform, &c);
			});
		ImGui::End();
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<EditorApp>();
}
