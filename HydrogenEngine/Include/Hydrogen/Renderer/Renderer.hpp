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
		static void Init(const std::shared_ptr<RenderContext>& renderContext);
		static void Shutdown();

		static void BeginFrame();
		static void EndFrame();
		static void Draw(const std::shared_ptr<Pipeline>& pipeline, const std::shared_ptr<Framebuffer>& framebuffer);
		static void DrawDebugGui(const std::shared_ptr<Pipeline>& pipeline, const std::shared_ptr<Framebuffer>& framebuffer, const std::shared_ptr<DebugGUI>& debugGUI);

		static const std::shared_ptr<RenderContext>& GetContext() { return s_RenderContext; }
		static const std::shared_ptr<RenderAPI>& GetAPI() { return s_RenderAPI; }
		static const std::shared_ptr<CommandQueue>& GetCommandQueue() { return s_CommandQueue; }

	private:
		static std::shared_ptr<RenderContext> s_RenderContext;
		static std::shared_ptr<RenderAPI> s_RenderAPI;
		static std::shared_ptr<CommandQueue> s_CommandQueue;
	};
}
