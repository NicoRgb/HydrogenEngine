#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"

using namespace Hydrogen;

VulkanTexture::VulkanTexture(const std::shared_ptr<RenderContext>& renderContext)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext))
{
}

VulkanTexture::~VulkanTexture()
{
}
