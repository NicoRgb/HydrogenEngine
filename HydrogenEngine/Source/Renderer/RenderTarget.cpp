#include "Hydrogen/Renderer/RenderTarget.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderTarget.hpp"

using namespace Hydrogen;

std::shared_ptr<RenderTarget> RenderTarget::Create(const std::shared_ptr<RenderContext>& renderContext, const RenderTargetSpec& spec)
{
	return std::make_shared<VulkanRenderTarget>(renderContext, spec);
}
