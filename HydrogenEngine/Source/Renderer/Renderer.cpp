#include "Hydrogen/Renderer/Renderer.hpp"

#include <tracy/Tracy.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Hydrogen;

#define MAX_TEXTURES 128
#define MAX_LIGHTS 16

Renderer::Renderer(const std::shared_ptr<RenderContext>& renderContext)
{
	m_RenderContext = renderContext;
	m_CommandBuffer = CommandBuffer::Create(m_RenderContext);
	m_DebugLinesVertexBuffer = DynamicVertexBuffer::Create(m_RenderContext, { {VertexElementType::Float3}, {VertexElementType::Float4} }, 20);
	m_DebugTrianglesVertexBuffer = DynamicVertexBuffer::Create(m_RenderContext, { {VertexElementType::Float3}, {VertexElementType::Float4} }, 20);

	m_DefaultTexture = Texture::Create(renderContext, TextureFormat::FormatR8G8B8A8, 1, 1);
	uint32_t data = 0xFFFFFFFF;
	m_DefaultTexture->UploadData(&data);
}

Renderer::~Renderer()
{
	m_RenderContext = nullptr;
	m_CommandBuffer = nullptr;
}

std::shared_ptr<Pipeline> Renderer::CreatePipeline(const std::shared_ptr<RenderTarget>& renderTarget, const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader)
{
	return Pipeline::Create(m_RenderContext, renderTarget, vertexShader, fragmentShader,
		{ {VertexElementType::Float3}, {VertexElementType::Float3}, {VertexElementType::Float2}, {VertexElementType::Float3} },
		{ { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex | ShaderStage::Fragment, sizeof(UniformBuffer), 1 },
		  { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES },
		  { 2, DescriptorType::StorageBuffer, ShaderStage::Fragment, sizeof(SceneLightsBuffer) + MAX_LIGHTS * sizeof(GPULight), 1 }, },
		{ { sizeof(PushConstants), ShaderStage::Vertex | ShaderStage::Fragment } }, Primitive::TRIANGLES);
}

void Renderer::CreateDebugPipelines(const std::shared_ptr<RenderTarget>& renderTarget, const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader)
{
	m_DebugLinesShader = Pipeline::Create(m_RenderContext, renderTarget, vertexShader, fragmentShader,
		{ { VertexElementType::Float3 }, { VertexElementType::Float4 } }, { { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex, sizeof(UniformBuffer), 1 } }, {}, Primitive::LINES);

	m_DebugTrianglesShader = Pipeline::Create(m_RenderContext, renderTarget, vertexShader, fragmentShader,
		{ { VertexElementType::Float3 }, { VertexElementType::Float4 } }, { { 0, DescriptorType::UniformBuffer, ShaderStage::Vertex, sizeof(UniformBuffer), 1 } }, {}, Primitive::TRIANGLES);
}

void Renderer::BeginFrame(const std::shared_ptr<RenderTarget>& renderTarget, CameraComponent& cameraComponent, glm::vec3 cameraPos)
{
	ZoneScopedN("Renderer::BeginFrame");

	m_FrameInfo.Pipelines.clear();
	m_FrameInfo.Textures.clear();
	m_FrameInfo.Objects.clear();

	m_FrameInfo._UniformBuffer.View = cameraComponent.View;
	m_FrameInfo._UniformBuffer.Proj = cameraComponent.Proj;
	m_FrameInfo._UniformBuffer.ViewPos = cameraPos;

	m_FrameInfo.NumDebugLineVertices = 0;
	m_FrameInfo.NumDebugTriangleVertices = 0;

	m_CurrentRenderTarget = renderTarget;

	{
		ZoneScopedN("RenderAPI::BeginFrame");
		m_CommandBuffer->BeginFrame(renderTarget);
	}
	{
		ZoneScopedN("Prepare Command Queue And Render Pass");
		m_CommandBuffer->StartRecording(renderTarget);
	}

	m_CommandBuffer->SetViewport(m_CurrentRenderTarget);
	m_CommandBuffer->SetScissor(m_CurrentRenderTarget);
}

void Renderer::BeginDebugGuiFrame(const std::shared_ptr<RenderTarget>& renderTarget)
{
	m_CurrentRenderTarget = renderTarget;
	m_CommandBuffer->BeginFrame(renderTarget);
	m_CommandBuffer->StartRecording(renderTarget);
}

void Renderer::EndDebugGuiFrame()
{
	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
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

	if (m_FrameInfo.NumDebugLineVertices > 0)
	{
		m_CommandBuffer->BindPipeline(m_DebugLinesShader);
		m_CommandBuffer->BindDynamicVertexBuffer(m_DebugLinesVertexBuffer);
		m_CommandBuffer->Draw(m_FrameInfo.NumDebugLineVertices);
	}

	if (m_FrameInfo.NumDebugTriangleVertices > 0)
	{
		m_CommandBuffer->BindPipeline(m_DebugTrianglesShader);
		m_CommandBuffer->BindDynamicVertexBuffer(m_DebugTrianglesVertexBuffer);
		m_CommandBuffer->Draw(m_FrameInfo.NumDebugTriangleVertices);
	}

	m_CommandBuffer->EndRecording();
	m_CommandBuffer->EndFrame();
}

#include "Hydrogen/Application.hpp"

void Renderer::Draw(const MeshRendererComponent& meshRenderer, const std::shared_ptr<Pipeline>& pipeline, const glm::mat4& transform)
{
	if (std::find(m_FrameInfo.Pipelines.begin(), m_FrameInfo.Pipelines.end(), pipeline) == m_FrameInfo.Pipelines.end())
	{
		m_FrameInfo.Pipelines.push_back(pipeline);
		pipeline->UploadUniformBufferData(0, &m_FrameInfo._UniformBuffer, sizeof(UniformBuffer));

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

		pipeline->UploadStorageBufferData(2, data, bufferSize);
		delete[] data;

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

void Renderer::DrawDebugGui(const std::shared_ptr<DebugGUI>& debugGUI)
{
	debugGUI->Render(m_CommandBuffer);
}

void Renderer::DrawDebugLines(const std::vector<DebugVertex>& vertices)
{
	m_FrameInfo.NumDebugLineVertices = vertices.size();
	m_DebugLinesVertexBuffer->Upload((void*)vertices.data(), vertices.size());

	m_DebugLinesShader->UploadUniformBufferData(0, &m_FrameInfo._UniformBuffer, sizeof(UniformBuffer));
}

void Renderer::DrawDebugTriangles(const std::vector<DebugVertex>& vertices)
{
	m_FrameInfo.NumDebugTriangleVertices = vertices.size();
	m_DebugTrianglesVertexBuffer->Upload((void*)vertices.data(), vertices.size());

	m_DebugTrianglesShader->UploadUniformBufferData(0, &m_FrameInfo._UniformBuffer, sizeof(UniformBuffer));
}
