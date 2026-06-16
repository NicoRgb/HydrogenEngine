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

	m_FrameGraph = FrameGraph::Create(renderContext, viewport->GetWidth(), viewport->GetHeight());

	FramePass debugGUIPass;
	debugGUIPass.AddResource("SceneColor",
		{
			.Type = FrameAttachmentType::Color,
			.SampleCount = 1,
			.Format = TextureFormat::ViewportDefault,
			.Sampled = false,
			.Cleared = true,
			.IsSwapChainAttachment = true
		});
	debugGUIPass.AddResource("SceneDepth",
		{
			.Type = FrameAttachmentType::Depth,
			.SampleCount = 1,
			.Format = TextureFormat::FormatD32Float,
			.Sampled = false,
			.Cleared = true
		});

	debugGUIPass.SetRender([this](const std::shared_ptr<FrameGraph>& graph)
		{
			m_CommandBuffer->BeginFrame(m_FrameGraph, "DebugGUIPass");
			m_CommandBuffer->StartRecording();
			m_DebugGUI->Render(m_CommandBuffer);
			m_CommandBuffer->EndRecording();
			m_CommandBuffer->EndFrame();
		});

	m_FrameGraph->AddPass("DebugGUIPass", std::make_unique<FramePass>(debugGUIPass));
	m_FrameGraph->Compose();

	m_DebugGUI = DebugGUI::Create(m_RenderContext, m_FrameGraph, "DebugGUIPass");
}

DebugGUIRenderer::~DebugGUIRenderer()
{
	m_CommandBuffer = nullptr;
}

void DebugGUIRenderer::Render()
{
	m_FrameGraph->Render();
}

void DebugGUIRenderer::Resize(uint32_t width, uint32_t height)
{
	m_FrameGraph->Resize(width, height);
}
