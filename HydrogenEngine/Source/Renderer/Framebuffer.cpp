#include "Hydrogen/Renderer/Framebuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"

using namespace Hydrogen;

std::shared_ptr<Framebuffer> Framebuffer::Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<Texture>& texture)
{
	return std::make_shared<VulkanFramebuffer>(renderContext, renderPass, texture);
}
