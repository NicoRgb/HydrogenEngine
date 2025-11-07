#pragma once

#include "RenderContext.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Renderer/VertexBuffer.hpp"
#include "Hydrogen/Renderer/RenderPass.hpp"

namespace Hydrogen
{
	enum class DescriptorType
	{
		UniformBuffer,
		//StorageBuffer,
		CombinedImageSampler,
		//StorageImage,
		//Sampler,
		//SampledImage,
		//InputAttachment,
	};

	enum class ShaderStage : uint32_t
	{
		Vertex = 0x1,
		Fragment = 0x2,
		All = 0x7
	};

	struct DescriptorBinding
	{
		uint32_t binding;
		DescriptorType type;
		ShaderStage stageFlags;
		size_t size;
		size_t num_elements;
	};

	struct PushConstantsRange
	{
		size_t size;
		ShaderStage stageFlags;
	};

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
		virtual void UploadTextureSampler(uint32_t binding, uint32_t index, const std::shared_ptr<Texture> &texture) = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<Pipeline>& pipeline)
		{
			static_assert(std::is_base_of_v<Pipeline, T>);

			return std::dynamic_pointer_cast<T>(pipeline);
		}

		static std::shared_ptr<Pipeline> Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<ShaderAsset>& vertexShaderAsset, const std::shared_ptr<ShaderAsset>& fragmentShaderAsset, VertexLayout vertexLayout, const std::vector<DescriptorBinding> descriptorBindings, const std::vector<PushConstantsRange> pushConstantsRanges);
	};
}
