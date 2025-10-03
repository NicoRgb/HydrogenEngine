#pragma once

#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

namespace Hydrogen
{
	class VulkanTexture : public Texture
	{
	public:
		VulkanTexture(const std::shared_ptr<RenderContext>& renderContext, TextureFormat format, size_t width, size_t height);
		~VulkanTexture();

		ImTextureID GetImGuiImage() override;

		size_t GetWidth() const override { return m_Width; }
		size_t GetHeight() const override { return m_Height; }

		void Resize(size_t width, size_t height) override;

		VkImageView GetImageView() const { return m_ImageView; }

	private:
		void CreateTexture();

		const std::shared_ptr<VulkanRenderContext> m_RenderContext;

		size_t m_Width, m_Height;
		TextureFormat m_Format;

		VkDeviceMemory m_ImageMemory;

		VkImageView m_ImageView;
		VkImage m_Image;
		VkSampler m_Sampler;

		ImTextureID m_ImGuiImage = 0;
	};
}
