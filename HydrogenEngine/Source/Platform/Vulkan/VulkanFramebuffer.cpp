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

VulkanFramebuffer::VulkanFramebuffer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<Texture>& texture)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)), m_RenderPass(RenderPass::Get<VulkanRenderPass>(renderPass)), m_Texture(texture ? Texture::Get<VulkanTexture>(texture) : nullptr)
{
	CreateFramebuffers();
}

VulkanFramebuffer::~VulkanFramebuffer()
{
	for (auto framebuffer : m_Framebuffers)
	{
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	}

	vkDestroyImageView(m_RenderContext->GetDevice(), m_DepthImageView, nullptr);
	vkDestroyImage(m_RenderContext->GetDevice(), m_DepthImage, nullptr);
	vkFreeMemory(m_RenderContext->GetDevice(), m_DepthImageMemory, nullptr);
}

void VulkanFramebuffer::CreateFramebuffers()
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_D32_SFLOAT;
	if (m_Texture)
	{
		imageInfo.extent.width = (uint32_t)m_Texture->GetWidth();
		imageInfo.extent.height = (uint32_t)m_Texture->GetHeight();
	}
	else
	{
		imageInfo.extent.width = m_RenderContext->GetSwapChainExtent().width;
		imageInfo.extent.height = m_RenderContext->GetSwapChainExtent().height;
	}
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	HY_ASSERT(vkCreateImage(m_RenderContext->GetDevice(), &imageInfo, nullptr, &m_DepthImage) == VK_SUCCESS, "Failed to create vulkan view");

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_RenderContext->GetDevice(), m_DepthImage, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_RenderContext->GetPhysicalDevice());

	HY_ASSERT(vkAllocateMemory(m_RenderContext->GetDevice(), &allocInfo, nullptr, &m_DepthImageMemory) == VK_SUCCESS, "Failed to allocate vulkan memory");

	HY_ASSERT(vkBindImageMemory(m_RenderContext->GetDevice(), m_DepthImage, m_DepthImageMemory, 0) == VK_SUCCESS, "Failed to bind vulkan memory");

	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = m_DepthImage;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = VK_FORMAT_D32_SFLOAT;
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	HY_ASSERT(vkCreateImageView(m_RenderContext->GetDevice(), &createInfo, nullptr, &m_DepthImageView) == VK_SUCCESS, "Failed to create vulkan image view");

	if (m_Texture)
	{
		m_Framebuffers.resize(1);

		VkImageView attachments[] = { m_Texture->GetImageView(), m_DepthImageView };

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

	const auto& swapChainImageViews = m_RenderContext->GetSwapChainImageViews();
	m_Framebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		VkImageView attachments[] = { swapChainImageViews[i], m_DepthImageView };

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

	vkDestroyImageView(m_RenderContext->GetDevice(), m_DepthImageView, nullptr);
	vkDestroyImage(m_RenderContext->GetDevice(), m_DepthImage, nullptr);
	vkFreeMemory(m_RenderContext->GetDevice(), m_DepthImageMemory, nullptr);

	CreateFramebuffers();
}
