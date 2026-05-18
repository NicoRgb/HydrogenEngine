#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "Hydrogen/Renderer/DebugGUIRenderer.hpp"
#include "Hydrogen/Application.hpp"

#include <tracy/Tracy.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Hydrogen;

DebugGUIRenderer::DebugGUIRenderer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Viewport>& viewport)
{
	m_RenderContext = renderContext;
	m_CommandBuffer = CommandBuffer::Create(m_RenderContext);

	RenderGraphSpec spec;
	spec.Width = (uint32_t)viewport->GetWidth();
	spec.Height = (uint32_t)viewport->GetHeight();
	spec.Attachments = {
		{ AttachmentType::Color, 1, TextureFormat::ViewportDefault, false, true, true },
		{ AttachmentType::Depth, 1, TextureFormat::FormatD32Float, false, true, false }
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
