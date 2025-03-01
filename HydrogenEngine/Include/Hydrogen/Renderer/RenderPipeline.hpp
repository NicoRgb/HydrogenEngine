#pragma once

#include "RenderContext.hpp"

namespace Hydrogen
{
	class RenderPipeline
	{
	public:
		virtual ~RenderPipeline() = default;

		template<typename T>
		static const std::shared_ptr<T>& Get(const std::shared_ptr<RenderPipeline>& renderPipeline)
		{
			static_assert(!std::is_base_of_v<RenderPipeline, T>);

			return std::dynamic_pointer_cast<T>(renderPipeline);
		}

		static std::shared_ptr<RenderPipeline> Create(const std::shared_ptr<RenderContext>& renderContext, const std::string& vertexShaderSrc, const std::string& fragmentShaderSrc);
	};
}
