#include "Hydrogen/Platform/Vulkan/VulkanRenderTarget.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

VulkanRenderTarget::VulkanRenderTarget(const std::shared_ptr<RenderContext>& renderContext,
									   const RenderTargetSpec& spec)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)),
	  m_Spec(spec)
{
	m_SampleCount = 1;

	for (const auto& attachment : m_Spec.Attachments)
	{
		if (attachment.Type == AttachmentType::Color)
		{
			m_SampleCount = attachment.SampleCount;
			break;
		}
	}

	CreateAttachments();
	CreateRenderPass();
	CreateFramebuffers();
}

VulkanRenderTarget::~VulkanRenderTarget()
{
	vkDestroyRenderPass(m_RenderContext->GetDevice(), m_RenderPass, nullptr);
	for (auto framebuffer : m_Framebuffers)
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	DestroyAttachments();
}

void VulkanRenderTarget::CreateAttachments()
{
	VkSampleCountFlagBits vkSamples = static_cast<VkSampleCountFlagBits>(m_SampleCount);

	for (const auto& attachmentSpec : m_Spec.Attachments)
	{
		if (attachmentSpec.Type == AttachmentType::Color)
		{
			if (m_Spec.Type == RenderTargetType::SwapChain)
			{
				continue;
			}

			m_ColorImage = std::make_shared<VulkanTexture>(
				m_RenderContext,
				TextureFormat::FormatR8G8B8A8,
				m_Spec.Width, m_Spec.Height,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
				VK_IMAGE_USAGE_SAMPLED_BIT |
				VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				vkSamples);
		}
		else if (attachmentSpec.Type == AttachmentType::Depth)
		{
			m_DepthImage = std::make_shared<VulkanTexture>(
				m_RenderContext,
				TextureFormat::FormatD32Float,
				m_Spec.Width, m_Spec.Height,
				VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
				vkSamples);
		}
		else if (attachmentSpec.Type == AttachmentType::Resolve)
		{
			m_ResolveImage = std::make_shared<VulkanTexture>(
				m_RenderContext,
				TextureFormat::FormatR8G8B8A8,
				m_Spec.Width, m_Spec.Height,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
				VK_SAMPLE_COUNT_1_BIT);
		}
	}
}

void VulkanRenderTarget::CreateRenderPass()
{
	std::vector<VkAttachmentDescription> attachments;
	std::vector<VkAttachmentReference> colorRefs;
	VkAttachmentReference depthRef{};
	VkAttachmentReference resolveRef{};
	bool hasResolve = m_ResolveImage != nullptr;
	uint32_t attachmentIndex = 0;

	VkSampleCountFlagBits vkSamples = static_cast<VkSampleCountFlagBits>(m_SampleCount);

	if (m_Spec.Type == RenderTargetType::SwapChain || m_ColorImage)
	{
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_Spec.Type == RenderTargetType::Texture ? m_ColorImage->GetFormat() : m_RenderContext->GetSwapChainImageFormat();
		colorAttachment.samples = vkSamples;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = hasResolve ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = m_Spec.Type == RenderTargetType::Texture ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference ref{};
		ref.attachment = attachmentIndex++;
		ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachments.push_back(colorAttachment);
		colorRefs.push_back(ref);
	}

	if (m_DepthImage)
	{
		VkAttachmentDescription depthAttachment{};
		depthAttachment.format = m_DepthImage->GetFormat();
		depthAttachment.samples = vkSamples;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthRef.attachment = attachmentIndex++;
		depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		attachments.push_back(depthAttachment);
	}

	if (m_ResolveImage)
	{
		VkAttachmentDescription resolveAttachment{};
		resolveAttachment.format = m_ResolveImage->GetFormat();
		resolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		resolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		resolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		resolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		resolveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		resolveRef.attachment = attachmentIndex++;
		resolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		attachments.push_back(resolveAttachment);
	}

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = colorRefs.size();
	subpass.pColorAttachments = colorRefs.data();
	if (m_DepthImage)
		subpass.pDepthStencilAttachment = &depthRef;
	if (hasResolve)
		subpass.pResolveAttachments = &resolveRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	HY_ASSERT(vkCreateRenderPass(m_RenderContext->GetDevice(), &renderPassInfo, nullptr, &m_RenderPass) == VK_SUCCESS, 
			 "Failed to create vulkan render pass");
}

void VulkanRenderTarget::CreateFramebuffers()
{
	switch (m_Spec.Type)
	{
	case RenderTargetType::SwapChain:
		CreateSwapChainFramebuffers();
		break;
	case RenderTargetType::Texture:
		CreateTextureFramebuffers();
		break;
	default:
		HY_ASSERT(false, "Unknown render target type");
	}
}

void VulkanRenderTarget::CreateSwapChainFramebuffers()
{
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = m_RenderPass;
	framebufferInfo.width = m_Spec.Width;
	framebufferInfo.height = m_Spec.Height;
	framebufferInfo.layers = 1;

	const auto& swapchainImageViews = m_RenderContext->GetSwapChainImageViews();

	m_Framebuffers.resize(swapchainImageViews.size());

	for (size_t i = 0; i < swapchainImageViews.size(); i++)
	{
		std::vector<VkImageView> attachments;
		attachments.push_back(swapchainImageViews[i]);

		if (m_DepthImage)
		{
			attachments.push_back(m_DepthImage->GetImageView());
		}

		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();

		HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) == VK_SUCCESS,
			"Failed to create vulkan framebuffer");
	}
}

void VulkanRenderTarget::CreateTextureFramebuffers()
{
	m_Framebuffers.resize(m_RenderContext->GetMaxFramesInFlight());

	for (size_t i = 0; i < m_Framebuffers.size(); i++)
	{
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass;
		framebufferInfo.width = m_Spec.Width;
		framebufferInfo.height = m_Spec.Height;
		framebufferInfo.layers = 1;

		std::vector<VkImageView> attachmentViews;
		if (m_ColorImage)
			attachmentViews.push_back(m_ColorImage->GetImageView());
		if (m_DepthImage)
			attachmentViews.push_back(m_DepthImage->GetImageView());
		if (m_ResolveImage)
			attachmentViews.push_back(m_ResolveImage->GetImageView());

		framebufferInfo.attachmentCount = attachmentViews.size();
		framebufferInfo.pAttachments = attachmentViews.data();

		HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) == VK_SUCCESS,
			"Failed to create vulkan framebuffer");
	}
}

void VulkanRenderTarget::OnResize(uint32_t width, uint32_t height)
{
	m_Spec.Width = width;
	m_Spec.Height = height;
	Invalidate();
}

void VulkanRenderTarget::Invalidate()
{
	vkDeviceWaitIdle(m_RenderContext->GetDevice());

	for (auto framebuffer : m_Framebuffers)
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	m_Framebuffers.clear();

	DestroyAttachments();
	CreateAttachments();
	CreateFramebuffers();
}

void VulkanRenderTarget::DestroyAttachments()
{
	m_ColorImage = nullptr;
	m_ResolveImage = nullptr;
	m_DepthImage = nullptr;
}
