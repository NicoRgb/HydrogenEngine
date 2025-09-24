#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"
#include "Hydrogen/Core.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"

#include "backends/imgui_impl_vulkan.h"

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

VulkanFramebuffer::VulkanFramebuffer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Pipeline>& pipeline, bool renderToTexture)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)), m_Pipeline(Pipeline::Get<VulkanPipeline>(pipeline)), m_RenderToTexture(renderToTexture)
{
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
	const auto& swapChainImageViews = m_RenderContext->GetSwapChainImageViews();

	if (m_RenderToTexture)
	{
		m_Framebuffers.resize(swapChainImageViews.size());
		m_Images.resize(swapChainImageViews.size());
		m_ImageViews.resize(swapChainImageViews.size());
		m_Samplers.resize(swapChainImageViews.size());
		m_ImguiTextures.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageCreateInfo imageInfo{};
			imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageInfo.imageType = VK_IMAGE_TYPE_2D;
			imageInfo.format = m_RenderContext->GetSwapChainImageFormat();
			imageInfo.extent.width = m_RenderContext->GetSwapChainExtent().width;
			imageInfo.extent.height = m_RenderContext->GetSwapChainExtent().height;
			imageInfo.extent.depth = 1;
			imageInfo.mipLevels = 1;
			imageInfo.arrayLayers = 1;
			imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			HY_ASSERT(vkCreateImage(m_RenderContext->GetDevice(), &imageInfo, nullptr, &m_Images[i]) == VK_SUCCESS, "Failed to create vulkan view");

			VkMemoryRequirements memRequirements;
			vkGetImageMemoryRequirements(m_RenderContext->GetDevice(), m_Images[i], &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = FindMemoryType(
				memRequirements.memoryTypeBits,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				m_RenderContext->GetPhysicalDevice());

			VkDeviceMemory imageMemory;
			HY_ASSERT(vkAllocateMemory(m_RenderContext->GetDevice(), &allocInfo, nullptr, &imageMemory) == VK_SUCCESS, "Failed to allocate vulkan memory");

			HY_ASSERT(vkBindImageMemory(m_RenderContext->GetDevice(), m_Images[i], imageMemory, 0) == VK_SUCCESS, "Failed to bind vulkan memory");

			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_Images[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_RenderContext->GetSwapChainImageFormat();
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			HY_ASSERT(vkCreateImageView(m_RenderContext->GetDevice(), &createInfo, nullptr, &m_ImageViews[i]) == VK_SUCCESS, "Failed to create vulkan image view");

			VkSamplerCreateInfo samplerInfo{};
			samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			samplerInfo.magFilter = VK_FILTER_LINEAR;
			samplerInfo.minFilter = VK_FILTER_LINEAR;
			samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

			vkCreateSampler(m_RenderContext->GetDevice(), &samplerInfo, nullptr, &m_Samplers[i]);

			VkImageView attachments[] = { m_ImageViews[i] };

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_Pipeline->GetRenderPass();
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = m_RenderContext->GetSwapChainExtent().width;
			framebufferInfo.height = m_RenderContext->GetSwapChainExtent().height;
			framebufferInfo.layers = 1;

			HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) == VK_SUCCESS, "Failed to create vulkan framebuffer");

			m_ImguiTextures[i] = ImGui_ImplVulkan_AddTexture(
				m_Samplers[i],
				m_ImageViews[i],
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);
		}

		return;
	}

	m_Framebuffers.resize(swapChainImageViews.size());

	for (size_t i = 0; i < swapChainImageViews.size(); i++)
	{
		VkImageView attachments[] = { swapChainImageViews[i] };

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_Pipeline->GetRenderPass();
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = m_RenderContext->GetSwapChainExtent().width;
		framebufferInfo.height = m_RenderContext->GetSwapChainExtent().height;
		framebufferInfo.layers = 1;

		HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) == VK_SUCCESS, "Failed to create vulkan framebuffer");
	}
}

void VulkanFramebuffer::OnResize(int width, int height)
{
	if (!Renderer::GetAPI()->FrameFinished())
	{
		Renderer::GetAPI()->GetFrameFinishedEvent().AddTempListener([this, width, height]() { OnResize(width, height); });
	}

	vkDeviceWaitIdle(m_RenderContext->GetDevice());

	for (auto framebuffer : m_Framebuffers)
	{
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	}
	m_RenderContext->OnResize(width, height);

	CreateFramebuffers();
}
