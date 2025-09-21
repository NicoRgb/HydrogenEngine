#pragma once

#include <memory>
#include "Hydrogen/Renderer/Pipeline.hpp"

namespace Hydrogen
{
	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void OnResize(int width, int height) = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<Framebuffer>& framebuffer)
		{
			static_assert(std::is_base_of_v<Framebuffer, T>);

			return std::dynamic_pointer_cast<T>(framebuffer);
		}

		static std::shared_ptr<Framebuffer> Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Pipeline>& pipeline);
	};
}
