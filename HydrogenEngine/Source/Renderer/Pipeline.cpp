#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanPipeline.hpp"

using namespace Hydrogen;

std::shared_ptr<Pipeline> Pipeline::Create(const std::shared_ptr<RenderContext>& renderContext, const std::string& vertexShaderSrc, const std::string& fragmentShaderSrc)
{
    return std::make_shared<VulkanPipeline>(renderContext, vertexShaderSrc, fragmentShaderSrc);
}
