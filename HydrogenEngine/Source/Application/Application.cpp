#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Camera.hpp"
#include "Hydrogen/Input.hpp"
#include "Hydrogen/ScriptEngine.hpp"

#include <ImGuizmo.h>
#include <reactphysics3d/reactphysics3d.h>

#ifdef HY_DEBUG
//#define TRACY_ENABLE
#endif

#include "tracy/Tracy.hpp"

using namespace Hydrogen;

Application* Application::s_Instance;

void Application::OnResize(int width, int height)
{
}

void Application::Run()
{
	OnSetup();
	ScriptEngine::Init();

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);
	MainViewport = Viewport::Create(ApplicationSpec.ViewportTitle, (int)ApplicationSpec.ViewportSize.x, (int)ApplicationSpec.ViewportSize.y, (int)ApplicationSpec.ViewportPos.x, (int)ApplicationSpec.ViewportPos.y);
	MainViewport->GetResizeEvent().AddListener(std::bind(&Application::OnResize, this, std::placeholders::_1, std::placeholders::_2));
	MainViewport->Open();

	Input::Initialize();

	_RenderContext = RenderContext::Create(ApplicationSpec.Name, ApplicationSpec.Version, MainViewport);
	MainAssetManager.LoadAssets("Assets", _RenderContext);

	CurrentScene = MainAssetManager.GetAsset<SceneAsset>("Scene.hyscene");
	CurrentScene->Load(&MainAssetManager);

	OnStartup();

	using clock = std::chrono::high_resolution_clock;
	auto lastTime = clock::now();

	accumulator = 0.0f;

	while (MainViewport->IsOpen())
	{
		auto currentTime = clock::now();
		std::chrono::duration<float> elapsed = currentTime - lastTime;
		float deltaTime = elapsed.count();
		lastTime = currentTime;

		Viewport::PumpMessages();

		if (MainViewport->GetWidth() == 0 || MainViewport->GetHeight() == 0)
		{
			continue;
		}

		OnUpdate(deltaTime);

		Input::EndFrame();
	}

	OnShutdown();
}

void Application::PhysicsUpdate(float deltaTime)
{
	float frameTime = deltaTime;

	if (frameTime > 0.03f) frameTime = 0.03f;
	accumulator += frameTime;

	while (accumulator >= timeStep)
	{
		CurrentScene->GetScene()->UpdatePhysics(timeStep);
		accumulator -= timeStep;
	}
	CurrentScene->GetScene()->Update(deltaTime);
}

void Application::RenderImGui(std::shared_ptr<DebugGUIRenderer>& ImGuiRenderer)
{
	ImGuiRenderer->GetDebugGUI()->BeginFrame();
	ImGuizmo::BeginFrame();

	static bool dockingEnabled = true;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	ImGui::Begin("DockSpace Demo", &dockingEnabled, window_flags);
	ImGui::PopStyleVar(2);

	ImGuiID dockspace_id = ImGui::GetID("DockSpace");
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	OnImGuiMenuBarRender();

	ImGui::End();

	OnImGuiRender();
	ImGuiRenderer->GetDebugGUI()->EndFrame();
}

void Application::SubmitImGui(std::shared_ptr<DebugGUIRenderer>& ImGuiRenderer)
{
	ImGuiRenderer->Render();

	auto& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void Application::ReloadShader()
{
}
