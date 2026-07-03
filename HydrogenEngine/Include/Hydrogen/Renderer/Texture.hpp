#pragma once

#include "Hydrogen/Renderer/RenderDevice.hpp"
#include <imgui.h>

namespace Hydrogen
{
	enum class TextureFormat
	{
		RGBA8_SRGB,
		RGBA16_SFLOAT,
		D32_SFLOAT,
		BGRA8_SRGB
	};

	enum class TextureUsage : uint32_t
	{
		SampledImage = 1 << 0,
		ColorAttachment = 1 << 1,
		DepthAttachment = 1 << 2
	};

	struct TextureDescription
	{
		uint32_t width = 0;
		uint32_t height = 0;
		TextureFormat format = TextureFormat::RGBA8_SRGB;
		TextureUsage usageFlags = TextureUsage::SampledImage;
	};

	class Texture
	{
	public:
		Texture(RenderDevice* device, const TextureDescription& desc);
		Texture(RenderDevice* device, VkImage image, VkImageView imageView, const TextureDescription& desc, VkSampler sampler = VK_NULL_HANDLE);
		~Texture();

		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;

		uint32_t GetWidth() const { return m_Desc.width; }
		uint32_t GetHeight() const { return m_Desc.height; }
		TextureFormat GetFormat() const { return m_Desc.format; }
		bool HasSampler() const { return (((uint32_t)m_Desc.usageFlags & (uint32_t)TextureUsage::SampledImage)) != 0; }

		VkImage GetImage() const { return m_Image; }
		VkImageView GetImageView() const { return m_ImageView; }

		ImTextureID GetImGuiTexture();

	private:
		uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
		void CreateSampler();

		RenderDevice* m_Device;

		TextureDescription m_Desc;

		VkImage m_Image = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VkSampler m_Sampler = VK_NULL_HANDLE;
		ImTextureID m_ImGuiImage = 0;
	};
}
