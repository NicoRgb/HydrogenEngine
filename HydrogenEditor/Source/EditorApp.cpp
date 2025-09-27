#include <Hydrogen/Hydrogen.hpp>
#include <imgui.h>

#include <backends/imgui_impl_vulkan.h>

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
		ImGui::Begin("Assets");

		ImGui::End();
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<EditorApp>();
}
