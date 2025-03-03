#pragma once

#include "RenderContext.hpp"

namespace Hydrogen
{
	class Pipeline
	{
	public:
		virtual ~Pipeline() = default;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<Pipeline>& renderPipeline)
		{
			static_assert(std::is_base_of_v<Pipeline, T>);

			return std::dynamic_pointer_cast<T>(renderPipeline);
		}

		static std::shared_ptr<Pipeline> Create(const std::shared_ptr<RenderContext>& renderContext, const std::string& vertexShaderSrc, const std::string& fragmentShaderSrc);
	};
}
