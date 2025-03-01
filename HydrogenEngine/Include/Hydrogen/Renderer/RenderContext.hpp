#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

#include "Hydrogen/Viewport.hpp"

namespace Hydrogen
{
	class RenderContext
	{
	public:
		virtual ~RenderContext() = default;
		
		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<RenderContext>& renderContext)
		{
			static_assert(std::is_base_of_v<RenderContext, T>);

			return std::dynamic_pointer_cast<T>(renderContext);
		}

		static std::shared_ptr<RenderContext> Create(std::string appName, glm::vec2 appVersion, const std::shared_ptr<Viewport>& viewport);
	};
}
