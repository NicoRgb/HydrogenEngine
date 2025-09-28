#pragma once

#include "RenderContext.hpp"
#include "Framebuffer.hpp"
#include "RenderAPI.hpp"
#include "CommandQueue.hpp"
#include "DebugGUI.hpp"

namespace Hydrogen
{
	class Renderer
	{
	public:
		Renderer(const std::shared_ptr<RenderContext>& renderContext);
		~Renderer();

		void BeginFrame(const std::shared_ptr<Framebuffer>& framebuffer);
		void EndFrame();
		void Draw(const std::shared_ptr<VertexBuffer>& vertexBuffer, const std::shared_ptr<IndexBuffer>& indexBuffer, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<Pipeline>& pipeline);
		void DrawDebugGui(const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<DebugGUI>& debugGUI);

		const std::shared_ptr<RenderContext>& GetContext() { return m_RenderContext; }
		const std::shared_ptr<RenderAPI>& GetAPI() { return m_RenderAPI; }
		const std::shared_ptr<CommandQueue>& GetCommandQueue() { return m_CommandQueue; }

	private:
		std::shared_ptr<RenderContext> m_RenderContext;
		std::shared_ptr<RenderAPI> m_RenderAPI;
		std::shared_ptr<CommandQueue> m_CommandQueue;

		std::shared_ptr<Framebuffer> m_CurrentFramebuffer;
	};
}
