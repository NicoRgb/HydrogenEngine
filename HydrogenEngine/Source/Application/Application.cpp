#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Camera.hpp"
#include "Hydrogen/Input.hpp"
#include "Hydrogen/ScriptEngine.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"

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

	Renderer MainRenderer(_RenderContext);
	Renderer ImGuiRenderer(_RenderContext);

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

void Application::RenderImGui(std::shared_ptr<DebugGUI>& debugGUI)
{
	if (debugGUI)
	{
		debugGUI->BeginFrame();
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
		debugGUI->EndFrame();
	}
}

void Application::SubmitImGui(std::shared_ptr<DebugGUI>& debugGUI, std::shared_ptr<Renderer>& ImGuiRenderer, std::shared_ptr<Framebuffer>& framebuffer, const std::shared_ptr<RenderPass>& renderPass)
{
	ImGuiRenderer->BeginDebugGuiFrame(framebuffer, renderPass);
	ImGuiRenderer->DrawDebugGui(debugGUI);
	ImGuiRenderer->EndDebugGuiFrame();

	auto& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void Application::Render(float deltaTime, std::shared_ptr<Renderer>& renderer, const std::shared_ptr<Pipeline>& pipeline, const std::shared_ptr<Framebuffer>& framebuffer, const std::shared_ptr<RenderPass>& renderPass, CameraComponent& camera, glm::vec3 cameraPos)
{
	/*const auto& lines = CurrentScene->GetScene()->GetPhysicsWorld().GetDebugLines();

	std::vector<Renderer::DebugVertex> debugLines;
	debugLines.reserve(lines.size() * 2);

	for (const auto& line : lines)
	{
		Renderer::DebugVertex v0, v1;

		v0.position = { line.point1.x, line.point1.y, line.point1.z };
		v1.position = { line.point2.x, line.point2.y, line.point2.z };

		v0.color = UnpackRP3DColor(line.color1);
		v1.color = UnpackRP3DColor(line.color2);

		debugLines.push_back(v0);
		debugLines.push_back(v1);
	}

	const auto& triangles = CurrentScene->GetScene()->GetPhysicsWorld().GetDebugTriangles();

	std::vector<Renderer::DebugVertex> debugTriangles;
	debugTriangles.reserve(triangles.size() * 3);

	for (const auto& triangles : triangles)
	{
		Renderer::DebugVertex v0, v1, v2;

		v0.position = { triangles.point1.x, triangles.point1.y, triangles.point1.z };
		v1.position = { triangles.point2.x, triangles.point2.y, triangles.point2.z };
		v2.position = { triangles.point3.x, triangles.point3.y, triangles.point3.z };

		v0.color = UnpackRP3DColor(triangles.color1);
		v1.color = UnpackRP3DColor(triangles.color2);
		v2.color = UnpackRP3DColor(triangles.color3);

		debugTriangles.push_back(v0);
		debugTriangles.push_back(v1);
		debugTriangles.push_back(v2);
	}*/

	renderer->BeginFrame(framebuffer, renderPass, camera, cameraPos);

	CurrentScene->GetScene()->IterateComponents<TransformComponent, MeshRendererComponent>([&](Entity entity, const TransformComponent& transform, const MeshRendererComponent& mesh)
		{
			(void)entity;
			if (mesh.Mesh)
			{
				renderer->Draw(mesh, pipeline, transform.Transform);
			}
		});

	//renderer.DrawDebugLines(debugLines);
	//renderer.DrawDebugTriangles(debugTriangles);
	renderer->EndFrame();
}

void Application::ReloadShader()
{
}
