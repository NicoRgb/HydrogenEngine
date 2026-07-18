#pragma once

#include <vulkan/vulkan.h>
#include "Hydrogen/Renderer/RenderDevice.hpp"
#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Core.hpp"

namespace Hydrogen
{
	enum class PresentMode
	{
		Immediate,
		VSync,
		Mailbox
	};

	enum class ColorFormat
	{
		RGBA8_SRGB,
		BGRA8_SRGB,
		HDR10
	};

	struct SwapChainSpec
	{
		PresentMode VsyncPreference = PresentMode::Mailbox;
		ColorFormat ColorPreference = ColorFormat::BGRA8_SRGB;
	};

	class SwapChain
	{
	public:
		SwapChain(RenderDevice* device, VkSurfaceKHR surface, const SwapChainSpec& spec);
		~SwapChain();

		struct RgResourceHandle AcquireNextImage(class RenderGraph* renderGraph, VkSemaphore semaphore);
		uint32_t GetCurrentImageIndex() const { return m_CurrentImageIndex; }

		VkExtent2D GetExtent() const { return m_Extent; }
		VkSwapchainKHR GetVulkanSwapchain() const { return m_SwapChain; }
		VkFormat GetVulkanImageFormat() const { return m_ImageFormat; }
		const std::vector<VkImage>& GetImages() const { return m_Images; }
		const std::vector<VkImageView>& GetImageViews() const { return m_ImageViews; }

		TextureFormat GetImageFormat() const
		{
			switch (m_ImageFormat)
			{
			case VK_FORMAT_R8G8B8A8_SRGB: return TextureFormat::RGBA8_SRGB; break;
			case VK_FORMAT_R16G16B16A16_SFLOAT: return TextureFormat::RGBA16_SFLOAT; break;
			case VK_FORMAT_B8G8R8A8_SRGB: return TextureFormat::BGRA8_SRGB; break;
			case VK_FORMAT_D32_SFLOAT: return TextureFormat::D32_SFLOAT; break;
			default: HY_ASSERT(false, "Invalid swapchain format");
			}
		}

		uint32_t GetWidth() const { return m_Extent.width; }
		uint32_t GetHeight() const { return m_Extent.height; }

	private:
		void QueryCapabilities();
		void ChooseCapabilities();
		void CreateSwapChain();
		void RetrieveImages();
		void CreateImageViews();

		RenderDevice* m_Device;
		VkSurfaceKHR m_Surface;
		SwapChainSpec m_Spec;

		VkSurfaceCapabilitiesKHR m_Capabilities;
		std::vector<VkSurfaceFormatKHR> m_Formats;
		std::vector<VkPresentModeKHR> m_PresentModes;

		VkExtent2D m_Extent;
		VkSurfaceFormatKHR m_Format;
		VkPresentModeKHR m_PresentMode;
		VkFormat m_ImageFormat;

		VkSwapchainKHR m_SwapChain;
		std::vector<VkImage> m_Images;
		std::vector<VkImageView> m_ImageViews;

		uint32_t m_CurrentImageIndex;
	};
}
