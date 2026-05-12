#include "Hydrogen/Renderer/Renderer.hpp"
#include "Hydrogen/Application.hpp"

#include <tracy/Tracy.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Hydrogen;

#define MAX_TEXTURES 128
#define MAX_LIGHTS 16

Renderer::Renderer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Viewport>& viewport)
{
	m_RenderContext = renderContext;
	m_CommandBuffer = CommandBuffer::Create(m_RenderContext);

	m_DefaultTexture = Texture::Create(renderContext, TextureFormat::FormatR8G8B8A8, 1, 1);
	uint32_t data = 0xFFFFFFFF;
	m_DefaultTexture->UploadData(&data);

	uint32_t maxMsaaSamples = m_RenderContext->GetCapabilities().MaxMSAASamples;

	RenderGraphSpec spec;
	spec.Width = (uint32_t)viewport->GetWidth();
	spec.Height = (uint32_t)viewport->GetHeight();

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

	m_RenderGraph = RenderGraph::Create(m_RenderContext, spec);
}

Renderer::Renderer(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height)
{
	m_RenderContext = renderContext;
	m_CommandBuffer = CommandBuffer::Create(m_RenderContext);

	m_DefaultTexture = Texture::Create(renderContext, TextureFormat::FormatR8G8B8A8, 1, 1);
	uint32_t data = 0xFFFFFFFF;
	m_DefaultTexture->UploadData(&data);

	uint32_t maxMsaaSamples = m_RenderContext->GetCapabilities().MaxMSAASamples;

	RenderGraphSpec spec;
	spec.Width = width;
	spec.Height = height;

	if (maxMsaaSamples > 1)
	{
		spec.Attachments = {
			{ AttachmentType::Color, maxMsaaSamples, false, true, false },
			{ AttachmentType::Depth, maxMsaaSamples, false, true, false },
			{ AttachmentType::Resolve, 1, true, true, false }
		};
	}
	else
	{
		spec.Attachments = {
			{ AttachmentType::Color, 1, true, true, false },
			{ AttachmentType::Depth, 1, false, true, false }
		};
	}

	m_RenderGraph = RenderGraph::Create(m_RenderContext, spec);
	if (maxMsaaSamples > 1)
	{
		m_SampledTexture = m_RenderGraph->GetResolveTexture();
	}
	else
	{
		m_SampledTexture = m_RenderGraph->GetColorTexture();
	}
}

Renderer::~Renderer()
{
	m_CommandBuffer = nullptr;
}

void Renderer::Render(const std::shared_ptr<Scene>& scene, CameraComponent& cameraComponent, glm::vec3 cameraPos)
{
	uint32_t numLights = 0;
	Application::Get()->CurrentScene->GetScene()->IterateComponents<LightComponent>(
		[&](Entity, const LightComponent&) { numLights++; });

	size_t bufferSize = sizeof(SceneLightsBuffer) + numLights * sizeof(GPULight);
	uint32_t* data = new uint32_t[bufferSize / sizeof(uint32_t)]();

	SceneLightsBuffer* header = reinterpret_cast<SceneLightsBuffer*>(data);
	header->lightCount = numLights;

	GPULight* gpuLights = reinterpret_cast<GPULight*>(data + sizeof(SceneLightsBuffer) / sizeof(uint32_t));

	uint32_t idx = 0;
	Application::Get()->CurrentScene->GetScene()->IterateComponents<LightComponent>(
		[&](Entity e, const LightComponent& light)
		{
			if (idx >= numLights) return;

			const auto& transform = e.GetComponent<TransformComponent>();

			gpuLights[idx].position = glm::vec4(e.GetComponent<TransformComponent>().GetPosition(), 512.0f);
			gpuLights[idx].color = light.color;

			idx++;
		});

	// TODO: find camera
	BeginFrame(cameraComponent, cameraPos);

	Application::Get()->CurrentScene->GetScene()->IterateComponents<MeshRendererComponent>(
		[&](Entity e, const MeshRendererComponent& m)
		{
			Draw(m, e.GetComponent<TransformComponent>().Transform, data, bufferSize);
		});

	EndFrame();

	delete[] data;
}

void Renderer::Resize(uint32_t width, uint32_t height)
{
	m_RenderGraph->OnResize(width, height);

	uint32_t maxMsaaSamples = m_RenderContext->GetCapabilities().MaxMSAASamples;
	if (maxMsaaSamples > 1)
	{
		m_SampledTexture = m_RenderGraph->GetResolveTexture();
	}
	else
	{
		m_SampledTexture = m_RenderGraph->GetColorTexture();
	}
}

void Renderer::BeginFrame(CameraComponent& cameraComponent, glm::vec3 cameraPos)
{
	ZoneScopedN("Renderer::BeginFrame");

	m_FrameInfo.Pipelines.clear();
	m_FrameInfo.Textures.clear();
	m_FrameInfo.Objects.clear();

	m_FrameInfo._UniformBuffer.View = cameraComponent.View;
	m_FrameInfo._UniformBuffer.Proj = cameraComponent.Proj;
	m_FrameInfo._UniformBuffer.ViewPos = cameraPos;

	{
		ZoneScopedN("RenderAPI::BeginFrame");
		m_CommandBuffer->BeginFrame(m_RenderGraph);
	}
	{
		ZoneScopedN("Prepare Command Queue And Render Pass");
		m_CommandBuffer->StartRecording(m_RenderGraph);
	}

	m_CommandBuffer->SetViewport(m_RenderGraph);
	m_CommandBuffer->SetScissor(m_RenderGraph);
}

void Renderer::EndFrame()
{
	for (const auto& object : m_FrameInfo.Objects)
	{
		m_CommandBuffer->BindPipeline(object.Shader);
		m_CommandBuffer->BindVertexBuffer(object.VertexBuf);
		m_CommandBuffer->BindIndexBuffer(object.IndexBuf);

		PushConstants pc{};
		pc.Model = object.Transform;
		pc.TextureIndex = object.TextureIndex;

		m_CommandBuffer->UploadPushConstants(object.Shader, 0, (void*)&pc);
		m_CommandBuffer->DrawIndexed(object.IndexBuf);
	}

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

void Renderer::Draw(const MeshRendererComponent& meshRenderer, const glm::mat4& transform, uint32_t* lightData, size_t lightDataSize)
{
	const auto& pipeline = GetOrCreatePipeline(meshRenderer.VertexShader, meshRenderer.FragmentShader);

	if (std::find(m_FrameInfo.Pipelines.begin(), m_FrameInfo.Pipelines.end(), pipeline) == m_FrameInfo.Pipelines.end())
	{
		m_FrameInfo.Pipelines.push_back(pipeline);
		pipeline->UploadUniformBufferData(0, &m_FrameInfo._UniformBuffer, sizeof(UniformBuffer));
		pipeline->UploadStorageBufferData(2, lightData, lightDataSize);

		for (uint32_t i = 0; i < MAX_TEXTURES; i++)
		{
			pipeline->UploadTextureSampler(1, i, m_DefaultTexture);
		}
	}

	auto texture = meshRenderer.Texture->GetTexture();
	if (!texture)
	{
		texture = m_DefaultTexture;
	}

	uint32_t index = 0;
	auto it = std::find(m_FrameInfo.Textures.begin(), m_FrameInfo.Textures.end(), texture);
	if (it != m_FrameInfo.Textures.end())
	{
		index = std::distance(m_FrameInfo.Textures.begin(), it);
	}
	else
	{
		m_FrameInfo.Textures.push_back(texture);
		index = m_FrameInfo.Textures.size() - 1;

		pipeline->UploadTextureSampler(1, index, texture);
	}

	m_FrameInfo.Objects.push_back({ meshRenderer.Mesh->GetVertexBuffer(), meshRenderer.Mesh->GetIndexBuffer(), pipeline, transform, index });
}

const std::shared_ptr<Pipeline>& Renderer::GetOrCreatePipeline(const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader)
{
	PipelineKey key = PipelineKey{ std::filesystem::path(vertexShader->GetPath()).filename().string(),
						 std::filesystem::path(fragmentShader->GetPath()).filename().string() };

	if (m_Pipelines.find(key) != m_Pipelines.end())
	{
		return m_Pipelines[key];
	}

	m_Pipelines[key] =
		Pipeline::Create(m_RenderContext, m_RenderGraph, vertexShader, fragmentShader,
			{ {VertexElementType::Float3}, {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3} },
			{ { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment, sizeof(UniformBuffer), 1 },
			  { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES },
			  { 2, DescriptorType::StorageBuffer, ShaderStage::Fragment, sizeof(SceneLightsBuffer) + MAX_LIGHTS * sizeof(GPULight), 1 }, },
			{ { sizeof(PushConstants), ShaderStage::Vertex | ShaderStage::Fragment } }, Primitive::TRIANGLES);

	return m_Pipelines[key];
}

DebugGUIRenderer::DebugGUIRenderer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Viewport>& viewport)
{
	m_RenderContext = renderContext;
	m_CommandBuffer = CommandBuffer::Create(m_RenderContext);

	RenderGraphSpec spec;
	spec.Width = (uint32_t)viewport->GetWidth();
	spec.Height = (uint32_t)viewport->GetHeight();
	spec.Attachments = {
		{ AttachmentType::Color, 1, false, true, true },
		{ AttachmentType::Depth, 1, false, true, false }
	};

	m_RenderGraph = RenderGraph::Create(m_RenderContext, spec);
	m_DebugGUI = DebugGUI::Create(m_RenderContext, m_RenderGraph);
}

DebugGUIRenderer::~DebugGUIRenderer()
{
	m_CommandBuffer = nullptr;
}

void DebugGUIRenderer::Render()
{
	m_CommandBuffer->BeginFrame(m_RenderGraph);
	m_CommandBuffer->StartRecording(m_RenderGraph);
	m_DebugGUI->Render(m_CommandBuffer);
	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

void DebugGUIRenderer::Resize(uint32_t width, uint32_t height)
{
	m_RenderGraph->OnResize(width, height);
}
