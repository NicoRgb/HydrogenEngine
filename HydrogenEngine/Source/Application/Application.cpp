#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Camera.hpp"
#include "Hydrogen/Input.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"

#include <ImGuizmo.h>

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

	EngineLogger::Init();
	AppLogger::Init();

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);

	MainViewport = Viewport::Create(ApplicationSpec.ViewportTitle, (int)ApplicationSpec.ViewportSize.x, (int)ApplicationSpec.ViewportSize.y, (int)ApplicationSpec.ViewportPos.x, (int)ApplicationSpec.ViewportPos.y);
	MainViewport->GetResizeEvent().AddListener(std::bind(&Application::OnResize, this, std::placeholders::_1, std::placeholders::_2));
	MainViewport->Open();

	Input::Initialize();

	FreeCam.ViewportWidth = MainViewport->GetWidth();
	FreeCam.ViewportHeight = MainViewport->GetHeight();
	FreeCam.CalculateProj();

	_RenderContext = RenderContext::Create(ApplicationSpec.Name, ApplicationSpec.Version, MainViewport);
	MainAssetManager.LoadAssets("Assets", _RenderContext);

	CurrentScene = MainAssetManager.GetAsset<SceneAsset>("Scene.hyscene");
	CurrentScene->Load(&MainAssetManager);

	if (ApplicationSpec.UseDebugGUI)
	{
		ViewportTexture = Texture::Create(_RenderContext, TextureFormat::ViewportDefault, 1920, 1080);
	}

	_RenderPass = RenderPass::Create(_RenderContext);

	auto framebuffer = Framebuffer::Create(_RenderContext, _RenderPass);
	MainViewport->GetResizeEvent().AddListener([&framebuffer, this](int width, int height) { _RenderContext->OnResize(width, height); framebuffer->OnResize(width, height); });

	std::shared_ptr<RenderPass> renderPassTexture = nullptr;
	std::shared_ptr<DebugGUI> debugGUI = nullptr;

	Renderer MainRenderer(_RenderContext);
	Renderer ImGuiRenderer(_RenderContext);

	if (ApplicationSpec.UseDebugGUI)
	{
		renderPassTexture = RenderPass::Create(_RenderContext, ViewportTexture);
		debugGUI = DebugGUI::Create(_RenderContext, _RenderPass);
		ViewportFramebuffer = Framebuffer::Create(_RenderContext, renderPassTexture, ViewportTexture);
	}

	MainPipeline = MainRenderer.CreatePipeline(_RenderPass, MainAssetManager.GetAsset<ShaderAsset>("VertexShader.glsl"), MainAssetManager.GetAsset<ShaderAsset>("FragmentShader.glsl"));

	OnStartup();

	using clock = std::chrono::high_resolution_clock;
	auto lastTime = clock::now();

	while (MainViewport->IsOpen())
	{
		auto currentTime = clock::now();
		std::chrono::duration<float> elapsed = currentTime - lastTime;
		float deltaTime = elapsed.count();
		lastTime = currentTime;

		Viewport::PumpMessages();
		OnUpdate();

		if (!FreeCam.Active)
		{
			CurrentScene->GetScene()->Update(1.0f / 60.0f);
		}

		if (MainViewport->GetWidth() == 0 || MainViewport->GetHeight() == 0)
		{
			continue;
		}

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

		Entity cameraEntity;
		CurrentScene->GetScene()->IterateComponents<CameraComponent>([&](Entity entity, CameraComponent& camera)
			{
				if (camera.Active)
				{
					cameraEntity = entity;
				}
			});

		if (cameraEntity.IsValid() || FreeCam.Active)
		{
			CameraComponent cameraComponent;

			if (FreeCam.Active)
			{
				FreeCam.Update(deltaTime);
				FreeCam.CalculateView();

				auto width = debugGUI ? ViewportTexture->GetWidth() : MainViewport->GetWidth();
				auto height = debugGUI ? ViewportTexture->GetHeight() : MainViewport->GetHeight();

				if (width != FreeCam.ViewportWidth || height != FreeCam.ViewportHeight)
				{
					FreeCam.ViewportWidth = width;
					FreeCam.ViewportHeight = height;
					FreeCam.CalculateProj();
				}
			}
			else
			{
				cameraComponent = cameraEntity.GetComponent<CameraComponent>();
				cameraComponent.CalculateView(cameraEntity);

				auto width = debugGUI ? ViewportTexture->GetWidth() : MainViewport->GetWidth();
				auto height = debugGUI ? ViewportTexture->GetHeight() : MainViewport->GetHeight();

				if (width != cameraComponent.ViewportWidth || height != cameraComponent.ViewportHeight)
				{
					cameraComponent.ViewportWidth = width;
					cameraComponent.ViewportHeight = height;
					cameraComponent.CalculateProj();
				}
			}

			MainRenderer.BeginFrame(debugGUI ? ViewportFramebuffer : framebuffer, debugGUI ? renderPassTexture : _RenderPass, FreeCam.Active ? FreeCam : cameraComponent);

			CurrentScene->GetScene()->IterateComponents<TransformComponent, MeshRendererComponent>([&](Entity entity, const TransformComponent& transform, const MeshRendererComponent& mesh)
				{
					(void)entity;
					if (mesh.Mesh)
					{
						MainRenderer.Draw(mesh, MainPipeline, transform.Transform);
					}
				});

			MainRenderer.EndFrame();
		}

		if (debugGUI)
		{
			ImGuiRenderer.BeginDebugGuiFrame(framebuffer, _RenderPass);
			ImGuiRenderer.DrawDebugGui(debugGUI);
			ImGuiRenderer.EndFrame();

			auto& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		}

		Input::EndFrame();
	}

	OnShutdown();
}

void Application::ReloadShader()
{
}
