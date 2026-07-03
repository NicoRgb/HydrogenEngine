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
        size_t size;
        ShaderStage stageFlags;
    };

	struct PipelineSpec
	{
        std::string VertexMain = "main";
        std::string FragmentMain = "main";
		VkExtent2D ViewportExtent = { 800, 600 };
        VertexLayout VertexBufferLayout = {};
        PrimitiveStyle Primitive = PrimitiveStyle::Triangles;
        ShaderCullMode CullMode = ShaderCullMode::None;
        BlendMode ColorBlending = BlendMode::None;
        std::vector<PushConstantsRange> PushConstants = {};

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
            HashCombine(seed, static_cast<size_t>(ColorBlending));
            HashCombine(seed, PushConstants.size());
            for (const auto& pc : PushConstants)
            {
                HashCombine(seed, pc.size);
                HashCombine(seed, static_cast<size_t>(pc.stageFlags));
            }

            return seed;
        }
	};

	class Pipeline
	{
	public:
		Pipeline(RenderDevice* device, VkRenderPass renderPass, const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader, PipelineSpec spec);
		~Pipeline();

        VkPipeline GetPipeline() const { return m_Pipeline; }

	private:
		VkShaderModule CreateShaderModule(const std::vector<uint32_t>& byteCode);
        VkPipelineVertexInputStateCreateInfo CreateVertexInputStateCreateInfo();

		RenderDevice* m_Device;
        PipelineSpec m_Spec;

        std::vector<VkPushConstantRange> m_VkPushConstantsRanges;
        VkPipelineLayout m_Layout;
        VkPipeline m_Pipeline;
	};
}
