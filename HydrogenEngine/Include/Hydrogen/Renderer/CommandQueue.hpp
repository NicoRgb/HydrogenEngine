#pragma once

#include <memory>

#include "Hydrogen/Renderer/Framebuffer.hpp"

namespace Hydrogen
{
	class CommandQueue
	{
	public:
		virtual ~CommandQueue() = default;
		
		virtual void StartRecording(const std::shared_ptr<class RenderAPI>& renderAPI) = 0;
		virtual void EndRecording() = 0;

		virtual void BindPipeline(const std::shared_ptr<Pipeline>& pipeline, const std::shared_ptr<Framebuffer>& framebuffer) = 0;
		virtual void UnbindPipeline() = 0;

		virtual void SetViewport() = 0;
		virtual void SetScissor() = 0;

		virtual void Draw() = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<CommandQueue>& commandQueue)
		{
			static_assert(std::is_base_of_v<CommandQueue, T>);

			return std::dynamic_pointer_cast<T>(commandQueue);
		}

		static std::shared_ptr<CommandQueue> Create(const std::shared_ptr<RenderContext>& renderContext);
	};
}
