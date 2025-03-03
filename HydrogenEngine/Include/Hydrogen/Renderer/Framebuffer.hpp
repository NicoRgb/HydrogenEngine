#pragma once

#include <memory>
#include "Hydrogen/Renderer/Pipeline.hpp"

namespace Hydrogen
{
	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		static std::shared_ptr<Framebuffer> Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Pipeline>& pipeline);
	};
}
