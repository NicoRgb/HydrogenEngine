#pragma once

#include "Hydrogen/Renderer/RenderContext.hpp"
#include "RenderTargetSpec.hpp"

namespace Hydrogen
{
	class RenderTarget
	{
	public:
		virtual ~RenderTarget() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual const RenderTargetSpec& GetSpec() const = 0;
		
		virtual bool IsMultisampled() const = 0;
		virtual uint32_t GetSampleCount() const = 0;
		
		virtual void OnResize(uint32_t width, uint32_t height) = 0;
		virtual void Invalidate() = 0;

		virtual const std::shared_ptr<Texture> GetColorTexture() const = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<RenderTarget>& target)
		{
			static_assert(std::is_base_of_v<RenderTarget, T>);
			return std::dynamic_pointer_cast<T>(target);
		}

		static std::shared_ptr<RenderTarget> Create(
			const std::shared_ptr<RenderContext>& renderContext,
			const RenderTargetSpec& spec);
	};
}
