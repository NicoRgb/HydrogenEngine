#pragma once

#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Renderer/Texture.hpp"

namespace Hydrogen
{
	enum class AttachmentType
	{
		Color,
		Depth,
		DepthStencil,
		Resolve
	};

	struct AttachmentSpec
	{
		AttachmentType Type;
		uint32_t SampleCount = 1;
		bool Sampled = false;
		bool Clear = true;
		bool IsSwapChainAttachment = false;
	};

	struct RenderGraphSpec
	{
		uint32_t Width;
		uint32_t Height;
		std::vector<AttachmentSpec> Attachments;
	};

	class RenderGraph
	{
	public:
		virtual ~RenderGraph() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual const RenderGraphSpec& GetSpec() const = 0;
		
		virtual void OnResize(uint32_t width, uint32_t height) = 0;
		virtual void Invalidate() = 0;

		virtual std::shared_ptr<Texture> GetColorTexture() const = 0;
		virtual std::shared_ptr<Texture> GetDepthTexture() const = 0;
		virtual std::shared_ptr<Texture> GetResolveTexture() const = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<RenderGraph>& target)
		{
			static_assert(std::is_base_of_v<RenderGraph, T>);
			return std::dynamic_pointer_cast<T>(target);
		}

		static std::shared_ptr<RenderGraph> Create(
			const std::shared_ptr<RenderContext>& renderContext,
			const RenderGraphSpec& spec);
	};
}
