#pragma once

#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

#include "imgui.h"

namespace Hydrogen
{
	uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice);
	VkFormat TextureFormatToVkFormat(TextureFormat format, std::shared_ptr<VulkanRenderContext> renderContext);

	class VulkanTexture : public Texture
	{
	public:
		VulkanTexture(const std::shared_ptr<RenderContext>& renderContext, TextureFormat format, size_t width, size_t height, VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT);
		~VulkanTexture();

		ImTextureID GetImGuiImage() override;

		size_t GetWidth() const override { return m_Width; }
		size_t GetHeight() const override { return m_Height; }

		void Resize(size_t width, size_t height) override;
		void UploadData(void* data) override;

		TextureFormat GetFormat() const override { return m_TextureFormat; }

		VkImage GetImage() const { return m_Image; }
		VkImageView GetImageView() const { return m_ImageView; }
		VkSampler GetSampler() const { return m_Sampler; }
		VkFormat GetVkFormat() const { return m_Format; }

	private:
		void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CreateTexture();

		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		TextureFormat m_TextureFormat;

		size_t m_Width, m_Height;
		VkFormat m_Format;
		VkImageUsageFlags m_Usage;
		VkSampleCountFlagBits m_Samples;
		VkImageLayout m_FinalLayout;

		VkDeviceMemory m_ImageMemory;

		VkImageView m_ImageView;
		VkImage m_Image;
		VkSampler m_Sampler;

		ImTextureID m_ImGuiImage = 0;
	};
}
