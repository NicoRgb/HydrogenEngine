#include "Hydrogen/Renderer/Framebuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"

using namespace Hydrogen;

std::shared_ptr<Framebuffer> Framebuffer::Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Pipeline>& pipeline, bool renderToTexture)
{
	return std::make_shared<VulkanFramebuffer>(renderContext, pipeline, renderToTexture);
}
