#pragma once

#include "RenderContext.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Renderer/VertexBuffer.hpp"
#include "Hydrogen/Renderer/RenderGraph.hpp"

namespace Hydrogen
{
	inline ShaderStage operator|(ShaderStage a, ShaderStage b)
	{
		return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	inline ShaderStage operator&(ShaderStage a, ShaderStage b)
	{
		return static_cast<ShaderStage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}

	class Pipeline
	{
	public:
		virtual ~Pipeline() = default;

		virtual void UploadUniformBufferData(uint32_t binding, void* data, size_t size) = 0;
		virtual void UploadStorageBufferData(uint32_t binding, void* data, size_t size) = 0;
		virtual void UploadTextureSampler(uint32_t binding, uint32_t index, const std::shared_ptr<Texture>& texture) = 0;
		virtual void UploadTextureSampler(uint32_t binding, uint32_t index, const std::shared_ptr<CubeMap>& cubeMap) = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<Pipeline>& pipeline)
		{
			static_assert(std::is_base_of_v<Pipeline, T>);

			return std::dynamic_pointer_cast<T>(pipeline);
		}

		static std::shared_ptr<Pipeline> Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<FrameGraph>& frameGraph, std::string framePass, PipelineSpec spec);
	};
}
