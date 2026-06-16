#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanPipeline.hpp"

using namespace Hydrogen;

std::shared_ptr<Pipeline> Pipeline::Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<FrameGraph>& frameGraph, std::string framePass, PipelineSpec spec)
{
    return std::make_shared<VulkanPipeline>(renderContext, frameGraph, framePass, spec);
}
