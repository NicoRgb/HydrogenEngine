#pragma once

#include <memory>

#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Renderer/CommandQueue.hpp"

namespace Hydrogen
{
	class DebugGUI
	{
	public:
		virtual ~DebugGUI() = default;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<DebugGUI>& debugGUI)
		{
			static_assert(std::is_base_of_v<DebugGUI, T>);

			return std::dynamic_pointer_cast<T>(debugGUI);
		}

		virtual void BeginFrame() = 0;
		virtual void EndFrame() = 0;
		virtual void Render(const std::shared_ptr<CommandQueue>& commandQueue) = 0;

		static std::shared_ptr<DebugGUI> Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass);
	};
}
