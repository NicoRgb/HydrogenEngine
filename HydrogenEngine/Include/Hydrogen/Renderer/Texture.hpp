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

	enum class TextureType
	{
		Texture2D,
		CubeMap
	};

	struct TextureDescription
	{
		uint32_t Width = 0;
		uint32_t Height = 0;

		TextureFormat Format = TextureFormat::RGBA8_SRGB;
		TextureUsage UsageFlags = TextureUsage::SampledImage;
		TextureType Type = TextureType::Texture2D;
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

		uint32_t GetWidth() const { return m_Desc.Width; }
		uint32_t GetHeight() const { return m_Desc.Height; }
		TextureFormat GetFormat() const { return m_Desc.Format; }
		bool IsSampled() const { return (((uint32_t)m_Desc.UsageFlags & (uint32_t)TextureUsage::SampledImage)) != 0; }

		VkImage GetImage() const { return m_Image; }
		VkImageView GetImageView() const { return m_ImageView; }

	private:
		void ExtractVulkanFlags();
		void CreateImageVMA();
		void CreateImageView();

		RenderDevice* m_Device;

		TextureDescription m_Desc;

		VkFormat m_VkFormat;
		VkImageAspectFlags m_AspectMask;
		VkImageUsageFlags m_UsageFlags;

		VkImage m_Image = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = VK_NULL_HANDLE;
	};
}
