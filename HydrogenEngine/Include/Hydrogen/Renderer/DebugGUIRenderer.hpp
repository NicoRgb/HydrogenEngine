#pragma once

#include "RenderContext.hpp"
#include "CommandBuffer.hpp"
#include "RenderGraph.hpp"
#include "DebugGUI.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Camera.hpp"

namespace Hydrogen
{
	class DebugGUIRenderer
	{
	public:
		DebugGUIRenderer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Viewport>& viewport);
		~DebugGUIRenderer();

		void Render();
		void Resize(uint32_t width, uint32_t height);

		const std::shared_ptr<DebugGUI>& GetDebugGUI() const { return m_DebugGUI; }

	private:
		std::shared_ptr<RenderContext> m_RenderContext;
		std::shared_ptr<CommandBuffer> m_CommandBuffer;
		std::shared_ptr<RenderGraph> m_RenderGraph;
		std::shared_ptr<DebugGUI> m_DebugGUI;
	};
}
