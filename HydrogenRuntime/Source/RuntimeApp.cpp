#include <Hydrogen/HydrogenMain.hpp>
#include <imgui.h>
#include <vector>
#include <numeric>
#include <algorithm>

using namespace Hydrogen;

class RuntimeApp : public Application
{
private:
	template<typename TCamera>
	void UpdateCameraViewportSize(TCamera& camera, const glm::ivec2& size)
	{
		if (camera.ViewportWidth != size.x ||
			camera.ViewportHeight != size.y)
		{
			camera.ViewportWidth = size.x;
			camera.ViewportHeight = size.y;
			camera.CalculateProj();
		}
	}

	bool UpdateCamera(float deltaTime, Entity& outEntity)
	{
		auto scene = CurrentScene->GetScene();
		Entity activeCameraEntity;

		scene->IterateComponents<CameraComponent>(
			[&](Entity entity, CameraComponent& camera)
			{
				if (camera.Active)
					activeCameraEntity = entity;
			});

		glm::ivec2 size = {
				(int)MainViewport->GetHeight(),
				(int)MainViewport->GetWidth()
		};

		if (activeCameraEntity.IsValid())
		{
			auto& camera = activeCameraEntity.GetComponent<CameraComponent>();
			camera.CalculateView(activeCameraEntity);
			UpdateCameraViewportSize(camera, size);
			outEntity = activeCameraEntity;
			return true;
		}

		return false;
	}

	std::unique_ptr<Renderer> m_Renderer;

public:
	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Runtime";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Runtime";
		ApplicationSpec.ViewportSize = { 1920, 1080 };
		ApplicationSpec.UseDebugGUI = false;
	}

	virtual void OnStartup() override
	{
		m_Renderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());

		CurrentScene->GetScene()->CreateScripts();
	}

	virtual void OnShutdown() override
	{
		m_Renderer.reset();
	}

	virtual void OnUpdate(float dt) override
	{
		PhysicsUpdate(dt);
		RenderScene(dt);
	}

	virtual void OnImGuiRender() override
	{
	}

	virtual void OnSwapchainRecreation() override
	{
		m_Renderer->UpdateSwapChain(ActiveSwapChain.get());
	}

	virtual void OnRenderDeviceChangeStart() override
	{
		m_Renderer.reset();
	}

	virtual void OnRenderDeviceChangeFinish() override
	{
		m_Renderer = std::make_unique<Renderer>(MainViewport, ActiveRenderDevice.get(), ActiveSwapChain.get());
	}

private:
	void RenderScene(float dt)
	{
		Entity cameraEntity;
		if (!UpdateCamera(dt, cameraEntity))
		{
			return;
		}

		const auto& camera = cameraEntity.GetComponent<CameraComponent>();
		const auto& cameraPos = cameraEntity.GetComponent<TransformComponent>().GetPosition();
		const auto& scene = CurrentScene->GetScene();

		DefaultRenderer::RenderSceneDeferred(m_Renderer.get(), { .Display = { .Width = (uint64_t)MainViewport->GetWidth(), .Height = (uint64_t)MainViewport->GetHeight() }}, camera, cameraPos, scene);
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<RuntimeApp>();
}
