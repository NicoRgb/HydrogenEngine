#pragma once

#include "Hydrogen/Renderer/Framebuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderPass.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanImage
	{
	public:
		VulkanImage(const VulkanRenderContext* renderContext, VkFormat swapChainFormat); // hack for swapchain images
		VulkanImage(const std::shared_ptr<VulkanRenderContext>& renderContext, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT); // use shared pointer for convention
		~VulkanImage();

		const VkImage& GetImage() const { return m_Image; }
		const VkImageView& GetImageView() const { HY_ASSERT(m_ImageView != VK_NULL_HANDLE, "Image view is not created"); return m_ImageView; }
		const VkDeviceMemory& GetImageMemory() const { return m_ImageMemory; }
		const VkFormat& GetFormat() const { return m_Format; }

		void SetImage(const VkImage& image) { m_Image = image; } // unsafe but needed for swapchain images

		void RecreateImageView() { CreateImageView(); }

	private:
		void CreateImage();
		void CreateImageView();
		const VulkanRenderContext* m_RenderContext; // raw pointer is ok here because it should be there the entire lifetime. its needed because the rendercontexts contructors create images

		VkFormat m_Format;
		VkExtent2D m_Extent;
		VkImageUsageFlags m_Usage;
		VkSampleCountFlagBits m_Samples;
		VkImage m_Image;
		VkDeviceMemory m_ImageMemory = VK_NULL_HANDLE;
		VkImageView m_ImageView = VK_NULL_HANDLE;
	};

	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<Texture>& texture);
		~VulkanFramebuffer();

		bool RenderToTexture() { return m_Texture != nullptr; }
		const std::shared_ptr<Texture> GetTexture() override { return m_Texture; }

		void OnResize(int width, int height) override;

		const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_Framebuffers; }

	private:
		void CreateFramebuffers();

		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		const std::shared_ptr<VulkanRenderPass> m_RenderPass;
		const std::shared_ptr<VulkanTexture> m_Texture;

		std::vector<VkFramebuffer> m_Framebuffers;

		std::shared_ptr<VulkanImage> m_DepthImage;
	};
}
