#pragma once

#include <memory>

#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Renderer/Framebuffer.hpp"
#include "Hydrogen/Event.hpp"

namespace Hydrogen
{
	class RenderAPI
	{
	public:
		virtual ~RenderAPI() = default;
		
		virtual void BeginFrame(const std::shared_ptr<Framebuffer>& framebuffer) = 0;
		virtual void SubmitFrame(const std::shared_ptr<class CommandQueue>& commandQueue) = 0;

		virtual bool FrameFinished() = 0;
		virtual Event<>& GetFrameFinishedEvent() = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<RenderAPI>& renderAPI)
		{
			static_assert(std::is_base_of_v<RenderAPI, T>);

			return std::dynamic_pointer_cast<T>(renderAPI);
		}

		static std::shared_ptr<RenderAPI> Create(const std::shared_ptr<RenderContext>& renderContext);
	};
}
