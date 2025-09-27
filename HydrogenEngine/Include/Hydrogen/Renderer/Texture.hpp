#pragma once

#include <memory>

#include "Hydrogen/Renderer/RenderContext.hpp"

namespace Hydrogen
{
	class Texture
	{
	public:
		virtual ~Texture() = default;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<Texture>& texture)
		{
			static_assert(std::is_base_of_v<Texture, T>);

			return std::dynamic_pointer_cast<T>(texture);
		}

		static std::shared_ptr<Texture> Create(const std::shared_ptr<RenderContext>& renderContext);
	};
}
