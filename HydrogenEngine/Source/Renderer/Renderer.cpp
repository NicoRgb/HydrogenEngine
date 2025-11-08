#include "Hydrogen/Renderer/Renderer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Hydrogen;

#define MAX_TEXTURES 128

Renderer::Renderer(const std::shared_ptr<RenderContext>& renderContext)
{
	m_RenderContext = renderContext;
	m_RenderAPI = RenderAPI::Create(m_RenderContext);
	m_CommandQueue = CommandQueue::Create(renderContext);

	m_DefaultTexture = Texture::Create(renderContext, TextureFormat::FormatR8G8B8A8, 1, 1);
	uint32_t data = 0xFFFFFFFF;
	m_DefaultTexture->UploadData(&data);
}

Renderer::~Renderer()
{
	m_RenderContext = nullptr;
	m_RenderAPI = nullptr;
	m_CommandQueue = nullptr;
}

std::shared_ptr<Pipeline> Renderer::CreatePipeline(const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader)
{
	return Pipeline::Create(m_RenderContext, renderPass, vertexShader, fragmentShader,
		{ {VertexElementType::Float3}, {VertexElementType::Float3}, {VertexElementType::Float2} },
		{ {0, DescriptorType::UniformBuffer, ShaderStage::Vertex, sizeof(UniformBuffer), 1},
		  { 1, DescriptorType::CombinedImageSampler, ShaderStage::Fragment, 0, MAX_TEXTURES } },
		{ { sizeof(PushConstants), ShaderStage::Vertex | ShaderStage::Fragment } });
}

void Renderer::BeginFrame(const std::shared_ptr<Framebuffer>& framebuffer, const std::shared_ptr<RenderPass>& renderPass, CameraComponent& cameraComponent)
{
	m_FrameInfo.Pipelines.clear();
	m_FrameInfo.Textures.clear();

	m_FrameInfo._UniformBuffer.View = cameraComponent.View;
	m_FrameInfo._UniformBuffer.Proj = cameraComponent.Proj;

	m_CurrentFramebuffer = framebuffer;

	m_RenderAPI->BeginFrame(framebuffer);
	m_CommandQueue->StartRecording(m_RenderAPI);
	m_CommandQueue->BeginRenderPass(renderPass, m_CurrentFramebuffer);

	m_CommandQueue->SetViewport(m_CurrentFramebuffer);
	m_CommandQueue->SetScissor(m_CurrentFramebuffer);
}

void Renderer::BeginDebugGuiFrame(const std::shared_ptr<Framebuffer>& framebuffer, const std::shared_ptr<RenderPass>& renderPass)
{
	m_CurrentFramebuffer = framebuffer;

	m_RenderAPI->BeginFrame(framebuffer);
	m_CommandQueue->StartRecording(m_RenderAPI);
	m_CommandQueue->BeginRenderPass(renderPass, m_CurrentFramebuffer);

	m_CommandQueue->SetViewport(m_CurrentFramebuffer);
	m_CommandQueue->SetScissor(m_CurrentFramebuffer);
}

void Renderer::EndFrame()
{
	m_CommandQueue->EndRenderPass();
	m_CommandQueue->EndRecording();
	m_RenderAPI->SubmitFrame(m_CommandQueue);
}

void Renderer::Draw(const MeshRendererComponent& meshRenderer, const std::shared_ptr<Pipeline>& pipeline, const glm::mat4& transform)
{
	if (std::find(m_FrameInfo.Pipelines.begin(), m_FrameInfo.Pipelines.end(), pipeline) == m_FrameInfo.Pipelines.end())
	{
		m_FrameInfo.Pipelines.push_back(pipeline);
		pipeline->UploadUniformBufferData(0, &m_FrameInfo._UniformBuffer, sizeof(UniformBuffer));

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

	m_CommandQueue->BindPipeline(pipeline);
	m_CommandQueue->BindVertexBuffer(meshRenderer.Mesh->GetVertexBuffer());
	m_CommandQueue->BindIndexBuffer(meshRenderer.Mesh->GetIndexBuffer());

	PushConstants pc{};
	pc.Model = transform;
	pc.TextureIndex = index;

	m_CommandQueue->UploadPushConstants(pipeline, 0, (void*)&pc);
	m_CommandQueue->DrawIndexed(meshRenderer.Mesh->GetIndexBuffer());
}

void Renderer::DrawDebugGui(const std::shared_ptr<DebugGUI>& debugGUI)
{
	debugGUI->Render(m_CommandQueue);
}
