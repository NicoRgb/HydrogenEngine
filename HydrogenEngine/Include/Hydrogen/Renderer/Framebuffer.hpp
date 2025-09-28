#pragma once

#include <memory>
#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Renderer/Texture.hpp"

namespace Hydrogen
{
	class Framebuffer
	{
	public:
		virtual ~Framebuffer() = default;

		virtual void OnResize(int width, int height) = 0;
		virtual bool RenderToTexture() = 0;
		virtual const std::shared_ptr<Texture> GetTexture() = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<Framebuffer>& framebuffer)
		{
			static_assert(std::is_base_of_v<Framebuffer, T>);

			return std::dynamic_pointer_cast<T>(framebuffer);
		}

		static std::shared_ptr<Framebuffer> Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<Texture>& texture = nullptr);
	};
}
