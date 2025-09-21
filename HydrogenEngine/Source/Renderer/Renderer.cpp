#include "Hydrogen/Renderer/Renderer.hpp"

using namespace Hydrogen;

std::shared_ptr<RenderContext> Renderer::s_RenderContext = nullptr;
std::shared_ptr<RenderAPI> Renderer::s_RenderAPI = nullptr;
std::shared_ptr<CommandQueue> Renderer::s_CommandQueue;

void Renderer::Init(const std::shared_ptr<RenderContext>& renderContext)
{
	s_RenderContext = renderContext;
	s_RenderAPI = RenderAPI::Create(s_RenderContext);
	s_CommandQueue = CommandQueue::Create(renderContext);
}

void Renderer::Shutdown()
{
	s_RenderContext = nullptr;
	s_RenderAPI = nullptr;
	s_CommandQueue = nullptr;
}

void Renderer::BeginFrame()
{
	s_RenderAPI->BeginFrame();
	s_CommandQueue->StartRecording(s_RenderAPI);
}

void Renderer::EndFrame()
{
	s_CommandQueue->EndRecording();
	s_RenderAPI->SubmitFrame(s_CommandQueue);
}

void Renderer::Draw(const std::shared_ptr<Pipeline>& pipeline, const std::shared_ptr<Framebuffer>& framebuffer)
{
	s_CommandQueue->BindPipeline(pipeline, framebuffer);
	s_CommandQueue->SetViewport();
	s_CommandQueue->SetScissor();
	s_CommandQueue->Draw();
	s_CommandQueue->UnbindPipeline();
}
