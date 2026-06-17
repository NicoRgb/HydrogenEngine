#pragma once

#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Renderer/VertexBuffer.hpp"

namespace Hydrogen
{
	enum class DescriptorType
	{
		UniformBuffer,
		StorageBuffer,
		CombinedImageSampler,
		//StorageImage,
		//Sampler,
		//SampledImage,
		//InputAttachment,
	};

	enum class Primitive
	{
		Lines,
		Triangles
	};

	enum class BlendMode
	{
		None,
		Additive,
		Alpha
	};

	enum class ShaderStage : uint32_t
	{
		Vertex = 0x1,
		Fragment = 0x2,
		All = 0x7
	};

	enum class CullMode
	{
		None,
		Front,
		Back
	};

	struct DepthSpec
	{
		bool DepthTest;
		bool DepthWrite;
	};

	struct DescriptorBinding
	{
		uint32_t binding;
		DescriptorType type;
		ShaderStage stageFlags;
		size_t size;
		size_t numElements;
	};

	struct PushConstantsRange
	{
		size_t size;
		ShaderStage stageFlags;
	};

	struct PipelineSpec
	{
		std::shared_ptr<class ShaderAsset> vertexShaderAsset;
		std::shared_ptr<class ShaderAsset> fragmentShaderAsset;
		VertexLayout vertexLayout;
		std::vector<DescriptorBinding> descriptorBindings;
		std::vector<PushConstantsRange> pushConstantsRanges;
		Primitive primitive;
		CullMode cullMode;
		BlendMode blendMode;
		DepthSpec depthSpec;
	};

	enum class FrameAttachmentType
	{
		Color,
		Depth,
		DepthStencil,
		Resolve
	};

	struct FrameAttachmentHandle
	{
		FrameAttachmentType Type;
		uint32_t SampleCount;
		TextureFormat Format;
		bool Sampled = false;
		bool Cleared = false;
		bool IsSwapChainAttachment = false;
	};

	class FrameGraph;

	class FramePass
	{
	public:
		using RenderFn = std::function<void(const std::shared_ptr<FrameGraph>&)>;

		FramePass() = default;

		void AddResource(const std::string& name, FrameAttachmentHandle handle) { m_Resources[name] = handle; m_ResourceOrder.push_back(name); }
		FrameAttachmentHandle GetResource(const std::string& name) const { return m_Resources.at(name); }
		std::unordered_map<std::string, FrameAttachmentHandle>& GetResources() { return m_Resources; }
		std::vector<std::string>& GetResourceOrder() { return m_ResourceOrder; }

		void AddPipeline(const std::string& name, PipelineSpec spec) { m_PipelineSpecs[name] = spec; }
		std::shared_ptr<class Pipeline> GetPipeline(const std::string& name) const { return m_Pipelines.at(name); }

		std::unordered_map<std::string, PipelineSpec>& GetPipelineSpecs() { return m_PipelineSpecs; }
		std::unordered_map<std::string, std::shared_ptr<class Pipeline>>& GetPipelines() { return m_Pipelines; }

		void SetRender(RenderFn renderFunction) { m_RenderFunction = renderFunction; }
		void Render(const std::shared_ptr<FrameGraph>& graph) const { m_RenderFunction(graph); }

	private:
		std::unordered_map<std::string, FrameAttachmentHandle> m_Resources;
		std::vector<std::string> m_ResourceOrder;
		std::unordered_map<std::string, PipelineSpec> m_PipelineSpecs;
		std::unordered_map<std::string, std::shared_ptr<class Pipeline>> m_Pipelines;
		RenderFn m_RenderFunction;
	};

	class FrameGraph : public std::enable_shared_from_this<FrameGraph>
	{
	public:
		void AddPass(std::string name, std::unique_ptr<FramePass> pass) { m_Passes[name] = std::move(pass); m_PassExecutionOrder.push_back(name); }
		const FramePass* GetPass(const std::string& name) const { return m_Passes.at(name).get(); }

		void Render()
		{
			for (const auto& passName : m_PassExecutionOrder)
			{
				const auto& pass = m_Passes.at(passName);
				pass->Render(shared_from_this());
			}
		}

		virtual void Compose() = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;

		virtual std::shared_ptr<Texture> GetTexture(const std::string& resourceName) const = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<FrameGraph>& frameGraph)
		{
			static_assert(std::is_base_of_v<FrameGraph, T>);

			return std::dynamic_pointer_cast<T>(frameGraph);
		}

		static std::shared_ptr<FrameGraph> Create(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height);

	protected:
		std::unordered_map<std::string, std::unique_ptr<FramePass>> m_Passes;
		std::vector<std::string> m_PassExecutionOrder;
	};
}
