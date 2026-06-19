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

	MainAssetManager.LoadAssets("Assets");

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

void Application::ReloadShader()
{
}
