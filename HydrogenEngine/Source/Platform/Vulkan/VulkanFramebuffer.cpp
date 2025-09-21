#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"
#include "Hydrogen/Core.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"

using namespace Hydrogen;

VulkanFramebuffer::VulkanFramebuffer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Pipeline>& pipeline)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)), m_Pipeline(Pipeline::Get<VulkanPipeline>(pipeline))
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
