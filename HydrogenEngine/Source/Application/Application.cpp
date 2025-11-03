#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Hydrogen/Application.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"

using namespace Hydrogen;

struct UniformBuffer
{
	alignas(16) glm::mat4 View;
	alignas(16) glm::mat4 Proj;
};

void Application::OnResize(int width, int height)
{
}

void Application::Run()
{
	OnSetup();

	EngineLogger::Init();
	AppLogger::Init();

	MainViewport = Viewport::Create(ApplicationSpec.ViewportTitle, (int)ApplicationSpec.ViewportSize.x, (int)ApplicationSpec.ViewportSize.y, (int)ApplicationSpec.ViewportPos.x, (int)ApplicationSpec.ViewportPos.y);
	MainViewport->Open();

	MainAssetManager.LoadAssets("Assets");

	CurrentScene = std::make_shared<Scene>();

	_RenderContext = RenderContext::Create(ApplicationSpec.Name, ApplicationSpec.Version, MainViewport);

	MainViewport->GetResizeEvent().AddListener(std::bind(&Application::OnResize, this, std::placeholders::_1, std::placeholders::_2));

	std::shared_ptr<Texture> texture = nullptr;

	if (ApplicationSpec.UseDebugGUI)
	{
		texture = Texture::Create(_RenderContext, TextureFormat::ViewportDefault, 1920, 1080);
	}

	auto vikingRoomTextureAsset = MainAssetManager.GetAsset<TextureAsset>("viking_room_texture.png");
	auto vikingRoomTexture = Texture::Create(_RenderContext, TextureFormat::FormatR8G8B8A8, vikingRoomTextureAsset->GetWidth(), vikingRoomTextureAsset->GetHeight());
	vikingRoomTexture->UploadData((void*)vikingRoomTextureAsset->GetImage().data());

	auto vikingRoom = MainAssetManager.GetAsset<MeshAsset>("viking_room.obj");
	auto cube = MainAssetManager.GetAsset<MeshAsset>("cube.obj");

	auto renderPass = RenderPass::Create(_RenderContext);
	auto pipeline = Pipeline::Create(_RenderContext, renderPass, MainAssetManager.GetAsset<ShaderAsset>("VertexShader.glsl"), MainAssetManager.GetAsset<ShaderAsset>("FragmentShader.glsl"),
		{ {VertexElementType::Float3}, {VertexElementType::Float3}, {VertexElementType::Float2} },
		{ {0, DescriptorType::UniformBuffer, ShaderStage::Vertex, sizeof(UniformBuffer), nullptr}, { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, vikingRoomTexture } },
		{ { sizeof(glm::mat4), ShaderStage::Vertex } });

	auto framebuffer = Framebuffer::Create(_RenderContext, renderPass);
	MainViewport->GetResizeEvent().AddListener([&framebuffer, this](int width, int height) { _RenderContext->OnResize(width, height); framebuffer->OnResize(width, height); });

	auto vertexBuffer = VertexBuffer::Create(_RenderContext, { {VertexElementType::Float3}, {VertexElementType::Float3}, {VertexElementType::Float2} }, (void*)vikingRoom->GetVertices().data(), vikingRoom->GetVertices().size() / 5);
	auto indexBuffer = IndexBuffer::Create(_RenderContext, vikingRoom->GetIndices());

	auto cubeVertexBuffer = VertexBuffer::Create(_RenderContext, { {VertexElementType::Float3}, {VertexElementType::Float3}, {VertexElementType::Float2} }, (void*)cube->GetVertices().data(), cube->GetVertices().size() / 5);
	auto cubeIndexBuffer = IndexBuffer::Create(_RenderContext, cube->GetIndices());

	std::shared_ptr<RenderPass> renderPassTexture = nullptr;
	std::shared_ptr<DebugGUI> debugGUI = nullptr;
	std::shared_ptr<Framebuffer> framebufferTexture = nullptr;

	Renderer MainRenderer(_RenderContext);
	Renderer ImGuiRenderer(_RenderContext);

	if (ApplicationSpec.UseDebugGUI)
	{
		renderPassTexture = RenderPass::Create(_RenderContext, texture);
		debugGUI = DebugGUI::Create(_RenderContext, renderPass);
		framebufferTexture = Framebuffer::Create(_RenderContext, renderPassTexture, texture);
	}

	HY_APP_INFO("Initializing app '{}' - Version {}.{}", ApplicationSpec.Name, ApplicationSpec.Version.x, ApplicationSpec.Version.y);

	Entity e0(CurrentScene, "Cube");
	e0.AddComponent<MeshRendererComponent>(cubeVertexBuffer, cubeIndexBuffer, vikingRoomTexture);

	Entity e1(CurrentScene, "Viking Room");
	e1.AddComponent<MeshRendererComponent>(vertexBuffer, indexBuffer, vikingRoomTexture);

	OnStartup();
	while (MainViewport->IsOpen())
	{
		Viewport::PumpMessages();
		OnUpdate();

		if (MainViewport->GetWidth() == 0 || MainViewport->GetHeight() == 0)
		{
			continue;
		}

		if (debugGUI)
		{
			debugGUI->BeginFrame();
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

			ImGui::End();

			ImGui::Begin("Viewport");
			ImVec2 contentRegion = ImGui::GetContentRegionAvail();
			if (contentRegion.x != m_ViewportSize.x || contentRegion.y != m_ViewportSize.y)
			{
				m_ViewportSize = contentRegion;

				texture->Resize((size_t)contentRegion.x, (size_t)contentRegion.y);
				framebufferTexture->OnResize((int)contentRegion.x, (int)contentRegion.y);
			}

			ImGui::Image(texture->GetImGuiImage(), contentRegion);
			ImGui::End();

			OnImGuiRender();
			debugGUI->EndFrame();
		}

		UniformBuffer uniformBuffer{};
		uniformBuffer.View = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		uniformBuffer.Proj = glm::perspective(glm::radians(45.0f),
			(float)(debugGUI ? texture->GetWidth() : MainViewport->GetWidth()) / (float)(debugGUI ? texture->GetHeight() : MainViewport->GetHeight()), 0.1f, 10.0f);
		uniformBuffer.Proj[1][1] *= -1;

		MainRenderer.BeginFrame(debugGUI ? framebufferTexture : framebuffer, debugGUI ? renderPassTexture : renderPass);

		CurrentScene->IterateComponents<TransformComponent, MeshRendererComponent>([&](Entity entity, const TransformComponent& transform, const MeshRendererComponent& mesh)
			{
				(void)entity;
				pipeline->UploadUniformBufferData(0, &uniformBuffer, sizeof(UniformBuffer));
				MainRenderer.Draw(mesh.VertexBuf, mesh.IndexBuf, pipeline, transform.Transform);
			});

		MainRenderer.EndFrame();

		if (debugGUI)
		{
			ImGuiRenderer.BeginFrame(framebuffer, renderPass);
			ImGuiRenderer.DrawDebugGui(debugGUI);
			ImGuiRenderer.EndFrame();

			auto& io = ImGui::GetIO();
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
			}
		}
	}

	OnShutdown();
}
