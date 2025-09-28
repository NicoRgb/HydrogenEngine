#pragma once

#include <memory>
#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Renderer/Texture.hpp"

namespace Hydrogen
{
	class RenderPass
	{
	public:
		virtual ~RenderPass() = default;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<RenderPass>& renderPass)
		{
			static_assert(std::is_base_of_v<RenderPass, T>);

			return std::dynamic_pointer_cast<T>(renderPass);
		}

		static std::shared_ptr<RenderPass> Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Texture>& texture = nullptr);
	};
}
