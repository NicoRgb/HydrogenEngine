#include "Hydrogen/Renderer/RenderPass.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderPass.hpp"

using namespace Hydrogen;

std::shared_ptr<RenderPass> RenderPass::Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Texture>& texture)
{
	return std::make_shared<VulkanRenderPass>(renderContext, texture);
}
