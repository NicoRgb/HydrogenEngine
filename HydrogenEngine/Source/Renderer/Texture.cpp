#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"

using namespace Hydrogen;

std::shared_ptr<Texture> Texture::Create(const std::shared_ptr<RenderContext>& renderContext, TextureFormat format, size_t width, size_t height)
{
	return std::make_shared<VulkanTexture>(renderContext, format, width, height, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT);
}
