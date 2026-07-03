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

Application::~Application()
{
	MainViewport.reset();
	m_RenderDevice.reset();
}

void Application::OnResize(int width, int height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	if (m_SwapChain)
	{
		RecreateSwapchain(m_CurrentSwapChainSpec);
	}
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

	RenderInstanceCreateInfo createInfo{};
	createInfo.ApplicationName = ApplicationSpec.Name.c_str();
	createInfo.ApplicationVersion = VK_MAKE_VERSION(ApplicationSpec.Version.x, ApplicationSpec.Version.y, 0);
	createInfo.EngineName = "Hydrogen Engine";
	createInfo.EngineVersion = VK_MAKE_VERSION(1, 0, 0);

	m_RenderInstance = std::make_unique<RenderInstance>(createInfo, MainViewport);

	RenderDeviceDescriptor renderDeviceDesc = m_RenderInstance->ChooseRenderDevice(MainViewport);
	m_RenderDevice = std::make_unique<RenderDevice>(renderDeviceDesc, MainViewport);

	m_CurrentSwapChainSpec.ColorPreference = ColorFormat::RGBA8_SRGB;
	m_CurrentSwapChainSpec.VsyncPreference = PresentMode::Mailbox;
	m_SwapChain = std::make_unique<SwapChain>(m_RenderDevice.get(), MainViewport->GetVulkanSurface(), m_CurrentSwapChainSpec);
	m_Renderer = std::make_unique<Renderer>(MainViewport, m_RenderDevice.get(), m_SwapChain.get());

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

		m_Renderer->BeginImGuiFrame();
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

		ImGui::Begin("MainDockSpace", &dockingEnabled, window_flags);

		ImGui::PopStyleVar(2);
		ImGuiID dockspace_id = ImGui::GetID("DockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
		OnImGuiMenuBarRender();
		ImGui::End();

		OnImGuiRender();

		m_Renderer->Render();

		auto& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		Input::EndFrame();
	}

	OnShutdown();

	m_Renderer.reset();
	m_SwapChain.reset();
	m_RenderDevice.reset();
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

const RenderDeviceDescriptor& Application::GetCurrentRenderDeviceDesc() const
{
	return m_RenderDevice->GetDescriptor();
}

void Application::ChangeRenderDevice(const RenderDeviceDescriptor& desc)
{
	m_RenderDevice->WaitForIdle();

	m_Renderer.reset();
	m_SwapChain.reset();
	m_RenderDevice.reset();

	m_RenderDevice = std::make_unique<RenderDevice>(desc, MainViewport);

	m_SwapChain = std::make_unique<SwapChain>(m_RenderDevice.get(), MainViewport->GetVulkanSurface(), m_CurrentSwapChainSpec);
	m_Renderer = std::make_unique<Renderer>(MainViewport, m_RenderDevice.get(), m_SwapChain.get());

	m_Renderer->ClearCache();
}

void Application::RecreateSwapchain(SwapChainSpec swapChainSepc)
{
	m_CurrentSwapChainSpec = swapChainSepc;

	m_RenderDevice->WaitForIdle();

	m_Renderer.reset();
	m_SwapChain.reset();

	m_SwapChain = std::make_unique<SwapChain>(m_RenderDevice.get(), MainViewport->GetVulkanSurface(), m_CurrentSwapChainSpec);
	m_Renderer = std::make_unique<Renderer>(MainViewport, m_RenderDevice.get(), m_SwapChain.get());
}
