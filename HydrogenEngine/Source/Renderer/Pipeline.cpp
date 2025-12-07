#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanPipeline.hpp"

using namespace Hydrogen;

std::shared_ptr<Pipeline> Pipeline::Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<ShaderAsset>& vertexShaderAsset, const std::shared_ptr<ShaderAsset>& fragmentShaderAsset, VertexLayout vertexLayout, const std::vector<DescriptorBinding> descriptorBindings, const std::vector<PushConstantsRange> pushConstantsRanges, Primitive primitive)
{
    return std::make_shared<VulkanPipeline>(renderContext, renderPass, vertexShaderAsset, fragmentShaderAsset, vertexLayout, descriptorBindings, pushConstantsRanges, primitive);
}
