#include <Hydrogen/HydrogenMain.hpp>

using namespace Hydrogen;

class RuntimeApp : public Application
{
private:
	std::shared_ptr<DeferredRenderer> MainRenderer;
	PostProcessing MainPostProcessing;

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
				(int)MainViewport->GetWidth(),
				(int)MainViewport->GetHeight()
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
		CurrentScene->GetScene()->CreateScripts();
		MainRenderer = std::make_shared<DeferredRenderer>(_RenderContext, MainViewport->GetWidth(), MainViewport->GetHeight());
	}

	virtual void OnShutdown() override
	{
	}

	virtual void OnUpdate(float dt) override
	{
		PhysicsUpdate(dt);

		Entity cameraEntity;
		if (UpdateCamera(dt, cameraEntity))
		{
			MainRenderer->Render(CurrentScene->GetScene(), cameraEntity.GetComponent<CameraComponent>(), cameraEntity.GetComponent<TransformComponent>().GetPosition());
			MainPostProcessing.PostProcess(MainRenderer, MainViewport->GetWidth(), MainViewport->GetHeight());
		}
	}

	virtual void OnImGuiRender() override
	{
	}

	virtual void OnImGuiMenuBarRender() override
	{
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<RuntimeApp>();
}
