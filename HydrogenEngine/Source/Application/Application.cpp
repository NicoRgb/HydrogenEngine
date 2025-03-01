#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"

#include "Hydrogen/Renderer/RenderPipeline.hpp"

using namespace Hydrogen;

void Test(int width, int height)
{
	HY_ENGINE_INFO("Resize: {}, {}", width, height);
}

void Application::Run()
{
	OnSetup();

	EngineLogger::Init();
	AppLogger::Init();

	MainViewport = Viewport::Create(ApplicationSpec.ViewportTitle, (int)ApplicationSpec.ViewportSize.x, (int)ApplicationSpec.ViewportSize.y, (int)ApplicationSpec.ViewportPos.x, (int)ApplicationSpec.ViewportPos.y);
	MainViewport->Open();
	MainViewport->GetResizeEvent().AddListener(Test);

	auto renderContext = RenderContext::Create(ApplicationSpec.Name, ApplicationSpec.Version, MainViewport);
	auto renderPipeline = RenderPipeline::Create(renderContext,
		R"(
		#version 450

		layout(location = 0) out vec3 fragColor;

		vec2 positions[3] = vec2[](
			vec2(0.0, -0.5),
			vec2(0.5, 0.5),
			vec2(-0.5, 0.5)
		);

		vec3 colors[3] = vec3[](
			vec3(1.0, 0.0, 0.0),
			vec3(0.0, 1.0, 0.0),
			vec3(0.0, 0.0, 1.0)
		);

		void main() {
			gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
			fragColor = colors[gl_VertexIndex];
		}
		)",

		R"(
		#version 450

		layout(location = 0) in vec3 fragColor;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = vec4(fragColor, 1.0);
		}
		)"
	);

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);

	OnStartup();
	while (MainViewport->IsOpen())
	{
		Viewport::PumpMessages();
		OnUpdate();
	}
	OnShutdown();
}
