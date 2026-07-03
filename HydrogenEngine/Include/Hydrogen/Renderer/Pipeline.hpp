#pragma once

#include "Hydrogen/Renderer/SwapChain.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Core.hpp"

namespace Hydrogen
{
    enum class VertexElementType
    {
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4
    };

    struct VertexBufferElement
    {
        VertexElementType Type;
    };

    using VertexLayout = std::vector<VertexBufferElement>;

    enum class PrimitiveStyle
    {
        Lines,
        Triangles
    };

    enum class ShaderCullMode
    {
        None,
        Front,
        Back
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

    struct PushConstantsRange
    {
        size_t Size;
        ShaderStage StageFlags;
    };

	enum class DescriptorType
	{
		UniformBuffer,
		StorageBuffer,
		CombinedImageSampler
	};

	struct DescriptorBinding
	{
		uint32_t Binding;
		DescriptorType Type;
		uint32_t Count;
		ShaderStage StageFlags;
	};

    struct DescriptorBindingValue
    {
        DescriptorType Type;
        std::vector<RenderBuffer*> RenderBuffers;
        std::vector<Texture*> Textures;
    };

	struct PipelineSpec
	{
        std::string VertexMain = "main";
        std::string FragmentMain = "main";
		VkExtent2D ViewportExtent = { 800, 600 };
        VertexLayout VertexBufferLayout = {};
        PrimitiveStyle Primitive = PrimitiveStyle::Triangles;
        ShaderCullMode CullMode = ShaderCullMode::None;
        std::vector<BlendMode> ColorBlending = {};
        std::vector<PushConstantsRange> PushConstants = {};
		std::vector<VkDescriptorSetLayout> DescriptorSetLayouts = {};

        size_t Hash()
        {
            size_t seed = 0;
            HashCombine(seed, std::hash<std::string>{}(VertexMain));
            HashCombine(seed, std::hash<std::string>{}(FragmentMain));
            HashCombine(seed, static_cast<size_t>(ViewportExtent.width));
            HashCombine(seed, static_cast<size_t>(ViewportExtent.height));
            HashCombine(seed, VertexBufferLayout.size());
            for (const auto& element : VertexBufferLayout)
            {
                HashCombine(seed, static_cast<size_t>(element.Type));
            }
            HashCombine(seed, static_cast<size_t>(Primitive));
            HashCombine(seed, static_cast<size_t>(CullMode));
            HashCombine(seed, ColorBlending.size());
            for (const auto& blending : ColorBlending)
            {
                HashCombine(seed, static_cast<size_t>(blending));
            }
            HashCombine(seed, PushConstants.size());
            for (const auto& pc : PushConstants)
            {
                HashCombine(seed, pc.Size);
                HashCombine(seed, static_cast<size_t>(pc.StageFlags));
            }
			HashCombine(seed, DescriptorSetLayouts.size());
			for (const auto& layout : DescriptorSetLayouts)
			{
				HashCombine(seed, reinterpret_cast<size_t>(layout));
			}

            return seed;
        }
	};

	class Pipeline
	{
	public:
		Pipeline(RenderDevice* device, VkRenderPass renderPass, const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader, PipelineSpec spec);
		~Pipeline();

        VkPipelineLayout GetPipelineLayout() const { return m_Layout; }
        VkPipeline GetPipeline() const { return m_Pipeline; }
        static VkDescriptorSetLayout CreateDescriptorSetLayout(RenderDevice* device, const std::vector<DescriptorBinding>& bindings);

	private:
		VkShaderModule CreateShaderModule(const std::vector<uint32_t>& byteCode);

		RenderDevice* m_Device;
        PipelineSpec m_Spec;

        std::vector<VkPushConstantRange> m_VkPushConstantsRanges;
        VkPipelineLayout m_Layout;
        VkPipeline m_Pipeline;
	};
}
