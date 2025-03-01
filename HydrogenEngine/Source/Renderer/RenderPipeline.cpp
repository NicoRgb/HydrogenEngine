#include "Hydrogen/Renderer/RenderPipeline.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderPipeline.hpp"

using namespace Hydrogen;

std::shared_ptr<RenderPipeline> RenderPipeline::Create(const std::shared_ptr<RenderContext>& renderContext, const std::string& vertexShaderSrc, const std::string& fragmentShaderSrc)
{
    return std::make_shared<VulkanRenderPipeline>(renderContext, vertexShaderSrc, fragmentShaderSrc);
}
