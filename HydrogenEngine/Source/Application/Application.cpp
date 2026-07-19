#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Camera.hpp"
#include "Hydrogen/Input.hpp"
#include "Hydrogen/ScriptEngine.hpp"

#include <ImGuizmo.h>
#include <reactphysics3d/reactphysics3d.h>

#include "tracy/Tracy.hpp"

using namespace Hydrogen;

Application* Application::s_Instance;

Application::~Application()
{
}

void Application::OnResize(int width, int height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	if (ActiveSwapChain)
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
	ActiveRenderDevice = std::make_unique<RenderDevice>(renderDeviceDesc, MainViewport);

	m_CurrentSwapChainSpec.ColorPreference = ColorFormat::RGBA8_SRGB;
	m_CurrentSwapChainSpec.VsyncPreference = PresentMode::Mailbox;
	ActiveSwapChain = std::make_unique<SwapChain>(ActiveRenderDevice.get(), MainViewport->GetVulkanSurface(), m_CurrentSwapChainSpec);

	OnStartup();

	using clock = std::chrono::high_resolution_clock;
	auto lastTime = clock::now();

	accumulator = 0.0f;

	while (MainViewport->IsOpen())
	{
		ZoneScopedN("Frame");

		auto currentTime = clock::now();
		std::chrono::duration<float> elapsed = currentTime - lastTime;
		float deltaTime = elapsed.count();
		lastTime = currentTime;

		Viewport::PumpMessages();

		if (MainViewport->GetWidth() == 0 || MainViewport->GetHeight() == 0)
		{
			continue;
		}

		OnImGuiRender();

		OnUpdate(deltaTime);

		auto& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}

		Input::EndFrame();
	}

	ActiveRenderDevice->WaitForIdle();

	DefaultRenderer::Reset();
	CurrentScene->ClearScene();
	MainAssetManager.Clear();

	OnShutdown();

	ActiveSwapChain.reset();
	ActiveRenderDevice.reset();
	MainViewport.reset();
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
	return ActiveRenderDevice->GetDescriptor();
}

void Application::ChangeRenderDevice(const RenderDeviceDescriptor& desc)
{
	ActiveRenderDevice->WaitForIdle();

	OnRenderDeviceChangeStart();

	ActiveSwapChain.reset();
	ActiveRenderDevice.reset();

	ActiveRenderDevice = std::make_unique<RenderDevice>(desc, MainViewport);

	ActiveSwapChain = std::make_unique<SwapChain>(ActiveRenderDevice.get(), MainViewport->GetVulkanSurface(), m_CurrentSwapChainSpec);

	OnRenderDeviceChangeFinish();
}

void Application::RecreateSwapchain(SwapChainSpec swapChainSepc)
{
	m_CurrentSwapChainSpec = swapChainSepc;

	ActiveRenderDevice->WaitForIdle();

	ActiveSwapChain.reset();
	ActiveSwapChain = std::make_unique<SwapChain>(ActiveRenderDevice.get(), MainViewport->GetVulkanSurface(), m_CurrentSwapChainSpec);

	OnSwapchainRecreation();
}
