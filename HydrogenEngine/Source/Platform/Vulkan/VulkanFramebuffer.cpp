#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"
#include "Hydrogen/Core.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"

using namespace Hydrogen;

static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) &&
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	HY_ASSERT(false, "Failed to find suitable memory type");
	return 0;
}

static VkImageAspectFlags GetImageAspectMask(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return VK_IMAGE_ASPECT_DEPTH_BIT;
		default:
			return VK_IMAGE_ASPECT_COLOR_BIT;
	}
}

VulkanImage::VulkanImage(const VulkanRenderContext* renderContext, VkFormat swapChainFormat)
	: m_RenderContext(renderContext), m_Format(swapChainFormat)
{
}

VulkanImage::VulkanImage(const std::shared_ptr<VulkanRenderContext>& renderContext, VkFormat format, VkExtent2D extent, VkImageUsageFlags usage, VkSampleCountFlagBits samples)
	: m_RenderContext(renderContext.get()), m_Format(format), m_Extent(extent), m_Usage(usage), m_Samples(samples)
{
	CreateImage();

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_RenderContext->GetDevice(), m_Image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_RenderContext->GetPhysicalDevice());

	HY_ASSERT(vkAllocateMemory(m_RenderContext->GetDevice(), &allocInfo, nullptr, &m_ImageMemory) == VK_SUCCESS, "Failed to allocate vulkan memory");
	HY_ASSERT(vkBindImageMemory(m_RenderContext->GetDevice(), m_Image, m_ImageMemory, 0) == VK_SUCCESS, "Failed to bind vulkan memory");

	CreateImageView();
}

VulkanImage::~VulkanImage()
{
	vkDestroyImageView(m_RenderContext->GetDevice(), m_ImageView, nullptr);
	vkDestroyImage(m_RenderContext->GetDevice(), m_Image, nullptr);
	vkFreeMemory(m_RenderContext->GetDevice(), m_ImageMemory, nullptr);
}

void VulkanImage::CreateImage()
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = m_Format;
	imageInfo.extent.width = m_Extent.width;
	imageInfo.extent.height = m_Extent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = m_Samples;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = m_Usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	HY_ASSERT(vkCreateImage(m_RenderContext->GetDevice(), &imageInfo, nullptr, &m_Image) == VK_SUCCESS, "Failed to create vulkan image");
}

void VulkanImage::CreateImageView()
{
	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = m_Image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = m_Format;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = GetImageAspectMask(m_Format);
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	HY_ASSERT(vkCreateImageView(m_RenderContext->GetDevice(), &createInfo, nullptr, &m_ImageView) == VK_SUCCESS, "Failed to create vulkan image view");
}

VulkanFramebuffer::VulkanFramebuffer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<Texture>& texture)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)), m_RenderPass(RenderPass::Get<VulkanRenderPass>(renderPass)), m_Texture(texture ? Texture::Get<VulkanTexture>(texture) : nullptr)
{
	m_DepthImage = std::make_shared<VulkanImage>(
		m_RenderContext,
		VK_FORMAT_D32_SFLOAT,
		m_Texture ? VkExtent2D{ (uint32_t)m_Texture->GetWidth(), (uint32_t)m_Texture->GetHeight() } : m_RenderContext->GetSwapChainExtent(),
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	CreateFramebuffers();
}

VulkanFramebuffer::~VulkanFramebuffer()
{
	for (auto framebuffer : m_Framebuffers)
	{
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	}
}

void VulkanFramebuffer::CreateFramebuffers()
{
	if (m_Texture)
	{
		m_Framebuffers.resize(1);

		VkImageView attachments[] = { m_Texture->GetImageView(), m_DepthImage->GetImageView() };

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass->GetRenderPass();
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = (uint32_t)m_Texture->GetWidth();
		framebufferInfo.height = (uint32_t)m_Texture->GetHeight();
		framebufferInfo.layers = 1;

		HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[0]) == VK_SUCCESS, "Failed to create vulkan framebuffer");

		return;
	}

	const auto& swapChainImages = m_RenderContext->GetSwapChainImages();
	m_Framebuffers.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); i++)
	{
		VkImageView attachments[] = { swapChainImages[i]->GetImageView(), m_DepthImage->GetImageView() };

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass->GetRenderPass();
		framebufferInfo.attachmentCount = 2;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = m_RenderContext->GetSwapChainExtent().width;
		framebufferInfo.height = m_RenderContext->GetSwapChainExtent().height;
		framebufferInfo.layers = 1;

		HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) == VK_SUCCESS, "Failed to create vulkan framebuffer");
	}
}

void VulkanFramebuffer::OnResize(int width, int height)
{
	vkDeviceWaitIdle(m_RenderContext->GetDevice());

	for (auto framebuffer : m_Framebuffers)
	{
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	}

	m_DepthImage.reset();

	m_DepthImage = std::make_shared<VulkanImage>(
		m_RenderContext,
		VK_FORMAT_D32_SFLOAT,
		m_Texture ? VkExtent2D{ (uint32_t)m_Texture->GetWidth(), (uint32_t)m_Texture->GetHeight() } : m_RenderContext->GetSwapChainExtent(),
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	CreateFramebuffers();
}
