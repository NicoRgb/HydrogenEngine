#include "Hydrogen/Renderer/Renderer.hpp"

using namespace Hydrogen;

Renderer::Renderer(const std::shared_ptr<RenderContext>& renderContext)
{
	m_RenderContext = renderContext;
	m_RenderAPI = RenderAPI::Create(m_RenderContext);
	m_CommandQueue = CommandQueue::Create(renderContext);
}

Renderer::~Renderer()
{
	m_RenderContext = nullptr;
	m_RenderAPI = nullptr;
	m_CommandQueue = nullptr;
}

void Renderer::BeginFrame(const std::shared_ptr<Framebuffer>& framebuffer)
{
	m_CurrentFramebuffer = framebuffer;

	m_RenderAPI->BeginFrame(framebuffer);
	m_CommandQueue->StartRecording(m_RenderAPI);
}

void Renderer::EndFrame()
{
	m_CommandQueue->EndRecording();
	m_RenderAPI->SubmitFrame(m_CommandQueue);
}

void Renderer::Draw(const std::shared_ptr<VertexBuffer>& vertexBuffer, const std::shared_ptr<IndexBuffer>& indexBuffer, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<Pipeline>& pipeline)
{
	m_CommandQueue->BeginRenderPass(renderPass, m_CurrentFramebuffer);
	m_CommandQueue->SetViewport(m_CurrentFramebuffer);
	m_CommandQueue->SetScissor(m_CurrentFramebuffer);
	m_CommandQueue->BindPipeline(pipeline);
	m_CommandQueue->BindVertexBuffer(vertexBuffer);
	m_CommandQueue->BindIndexBuffer(indexBuffer);
	m_CommandQueue->DrawIndexed(indexBuffer);
	m_CommandQueue->EndRenderPass();
}

void Renderer::DrawDebugGui(const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<DebugGUI>& debugGUI)
{
	m_CommandQueue->BeginRenderPass(renderPass, m_CurrentFramebuffer);
	m_CommandQueue->SetViewport(m_CurrentFramebuffer);
	m_CommandQueue->SetScissor(m_CurrentFramebuffer);
	debugGUI->Render(m_CommandQueue);
	m_CommandQueue->EndRenderPass();
}
