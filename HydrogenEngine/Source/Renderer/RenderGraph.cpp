#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderGraph.hpp"

using namespace Hydrogen;

std::shared_ptr<FrameGraph> FrameGraph::Create(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height)
{
	return std::make_shared<VulkanFrameGraph>(renderContext, width, height);
}
