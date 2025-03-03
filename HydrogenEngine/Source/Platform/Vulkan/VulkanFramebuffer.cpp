#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

VulkanFramebuffer::VulkanFramebuffer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Pipeline>& pipeline)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)), m_Pipeline(Pipeline::Get<VulkanPipeline>(pipeline))
{
	const auto& swapChainImageViews = m_RenderContext->GetSwapChainImageViews();
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

VulkanFramebuffer::~VulkanFramebuffer()
{
	for (auto framebuffer : m_Framebuffers)
	{
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	}
}
