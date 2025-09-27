#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"

using namespace Hydrogen;

std::shared_ptr<Texture> Texture::Create(const std::shared_ptr<RenderContext>& renderContext)
{
	return std::make_shared<VulkanTexture>(renderContext);
}
