#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"

using namespace Hydrogen;

std::shared_ptr<Texture> Texture::Create(const std::shared_ptr<RenderContext>& renderContext, TextureFormat format, size_t width, size_t height)
{
	return std::make_shared<VulkanTexture>(renderContext, format, width, height);
}
