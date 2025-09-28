#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"
#include "Hydrogen/Core.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"

using namespace Hydrogen;

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
}

void VulkanFramebuffer::CreateFramebuffers()
{
	if (m_Texture)
	{
		m_Framebuffers.resize(1);

		VkImageView attachments[] = { m_Texture->GetImageView() };

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass->GetRenderPass();
		framebufferInfo.attachmentCount = 1;
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
		VkImageView attachments[] = { swapChainImageViews[i] };

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass->GetRenderPass();
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
	vkDeviceWaitIdle(m_RenderContext->GetDevice());

	for (auto framebuffer : m_Framebuffers)
	{
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	}

	CreateFramebuffers();
}
