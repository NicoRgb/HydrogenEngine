#include <Hydrogen/HydrogenMain.hpp>

using namespace Hydrogen;

class RuntimeApp : public Application
{
private:
	std::shared_ptr<RenderGraph> SceneRenderGraph;
	std::shared_ptr<Pipeline> DefaultPipeline;
	std::shared_ptr<Renderer> MainRenderer;

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

		uint32_t maxMsaaSamples = _RenderContext->GetCapabilities().MaxMSAASamples;

		RenderGraphSpec spec;
		spec.Width = (uint32_t)MainViewport->GetWidth();
		spec.Height = (uint32_t)MainViewport->GetHeight();

		if (maxMsaaSamples > 1)
		{
			spec.Attachments = {
				{ AttachmentType::Color, maxMsaaSamples, false, true, false },
				{ AttachmentType::Depth, maxMsaaSamples, false, true, false },
				{ AttachmentType::Resolve, 1, false, true, true }
			};
		}
		else
		{
			spec.Attachments = {
				{ AttachmentType::Color, 1, false, true, true },
				{ AttachmentType::Depth, 1, false, true, false }
			};
		}
		
		SceneRenderGraph = RenderGraph::Create(_RenderContext, spec);
		MainRenderer = std::make_shared<Renderer>(_RenderContext);
		DefaultPipeline =
			MainRenderer->CreatePipeline(
				SceneRenderGraph,
				MainAssetManager.GetAsset<ShaderAsset>("VertexShader.glsl"),
				MainAssetManager.GetAsset<ShaderAsset>("FragmentShader.glsl"));
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
			Render(dt,
				MainRenderer,
				DefaultPipeline,
				SceneRenderGraph,
				cameraEntity.GetComponent<CameraComponent>(),
				cameraEntity.GetComponent<TransformComponent>().GetPosition());
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
