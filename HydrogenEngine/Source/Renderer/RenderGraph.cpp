#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderGraph.hpp"

using namespace Hydrogen;

std::shared_ptr<RenderGraph> RenderGraph::Create(const std::shared_ptr<RenderContext>& renderContext, const RenderGraphSpec& spec)
{
	return std::make_shared<VulkanRenderGraph>(renderContext, spec);
}
