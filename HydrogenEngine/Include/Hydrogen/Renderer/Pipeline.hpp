#pragma once

#include "RenderContext.hpp"
#include "Hydrogen/AssetManager.hpp"

namespace Hydrogen
{
	class Pipeline
	{
	public:
		virtual ~Pipeline() = default;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<Pipeline>& pipeline)
		{
			static_assert(std::is_base_of_v<Pipeline, T>);

			return std::dynamic_pointer_cast<T>(pipeline);
		}

		static std::shared_ptr<Pipeline> Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<ShaderAsset>& vertexShaderAsset, const std::shared_ptr<ShaderAsset>& fragmentShaderAsset);
	};
}
