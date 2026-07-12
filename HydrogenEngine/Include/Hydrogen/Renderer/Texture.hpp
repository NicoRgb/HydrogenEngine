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
		Texture(RenderDevice* device, VkImage image, VkImageView imageView, const TextureDescription& desc);
		~Texture();

		void UploadData(uint32_t* data, uint32_t width, uint32_t height);

		Texture(const Texture&) = delete;
		Texture& operator=(const Texture&) = delete;

		uint32_t GetWidth() const { return m_Desc.width; }
		uint32_t GetHeight() const { return m_Desc.height; }
		TextureFormat GetFormat() const { return m_Desc.format; }
		bool IsSampled() const { return (((uint32_t)m_Desc.usageFlags & (uint32_t)TextureUsage::SampledImage)) != 0; }

		VkImage GetImage() const { return m_Image; }
		VkImageView GetImageView() const { return m_ImageView; }

	private:
		RenderDevice* m_Device;

		TextureDescription m_Desc;

		VkImage m_Image = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = VK_NULL_HANDLE;
	};
}
