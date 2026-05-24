#include "Hydrogen/Platform/Vulkan/VulkanRenderGraph.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

VkSampleCountFlagBits Hydrogen::GetVulkanSampleCount(uint32_t sampleCount)
{
	switch (sampleCount)
	{
	case 1: return VK_SAMPLE_COUNT_1_BIT;
	case 2: return VK_SAMPLE_COUNT_2_BIT;
	case 4: return VK_SAMPLE_COUNT_4_BIT;
	case 8: return VK_SAMPLE_COUNT_8_BIT;
	case 16: return VK_SAMPLE_COUNT_16_BIT;
	case 32: return VK_SAMPLE_COUNT_32_BIT;
	case 64: return VK_SAMPLE_COUNT_64_BIT;
	default:
		HY_ASSERT(false, "Unsupported sample count");
		return VK_SAMPLE_COUNT_1_BIT;
	}
}

VulkanRenderGraph::VulkanRenderGraph(const std::shared_ptr<RenderContext>& renderContext,
									   const RenderGraphSpec& spec)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)),
	  m_Spec(spec)
{
	CreateAttachments();
	CreateRenderPass();
	CreateFramebuffers();
}

VulkanRenderGraph::~VulkanRenderGraph()
{
	vkDestroyRenderPass(m_RenderContext->GetDevice(), m_RenderPass, nullptr);
	for (auto framebuffer : m_Framebuffers)
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	DestroyAttachments();
}

void VulkanRenderGraph::CreateAttachments()
{
	m_SampleCount = 1;
	m_ColorImages.clear();
	m_ResolveImages.clear();

	for (const auto& attachmentSpec : m_Spec.Attachments)
	{
		if (attachmentSpec.Type == AttachmentType::Color)
		{
			m_SampleCount = attachmentSpec.SampleCount;
			if (attachmentSpec.IsSwapChainAttachment || attachmentSpec.Texture)
			{
				continue;
			}

			VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (attachmentSpec.Sampled)
			{
				usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			auto texture = std::make_shared<VulkanTexture>(
				m_RenderContext,
				attachmentSpec.Format,
				m_Spec.Width, m_Spec.Height,
				finalLayout,
				usage,
				GetVulkanSampleCount(attachmentSpec.SampleCount));

			m_ColorImages.push_back(texture);
		}
		else if (attachmentSpec.Type == AttachmentType::Depth)
		{
			HY_ASSERT(attachmentSpec.IsSwapChainAttachment == false, "Depth attachments cannot be swap chain attachments");
			if (attachmentSpec.Texture)
			{
				continue;
			}

			VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			if (attachmentSpec.Sampled)
			{
				usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
				finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			m_DepthImage = std::make_shared<VulkanTexture>(
				m_RenderContext,
				attachmentSpec.Format, //TextureFormat::FormatD32Float,
				m_Spec.Width, m_Spec.Height,
				finalLayout,
				usage,
				GetVulkanSampleCount(attachmentSpec.SampleCount));
		}
		else if (attachmentSpec.Type == AttachmentType::DepthStencil)
		{
			HY_ASSERT(attachmentSpec.IsSwapChainAttachment == false, "Depth-stencil attachments cannot be swap chain attachments");
			if (attachmentSpec.Texture)
			{
				continue;
			}

			VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
			VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			if (attachmentSpec.Sampled)
			{
				usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
				finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			m_DepthImage = std::make_shared<VulkanTexture>(
				m_RenderContext,
				attachmentSpec.Format, // TextureFormat::FormatD32Float,
				m_Spec.Width, m_Spec.Height,
				finalLayout,
				usage,
				GetVulkanSampleCount(attachmentSpec.SampleCount));
		}
		else if (attachmentSpec.Type == AttachmentType::Resolve && !attachmentSpec.IsSwapChainAttachment && !attachmentSpec.Texture)
		{
			VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (attachmentSpec.Sampled)
			{
				usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
				finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			HY_ASSERT(attachmentSpec.SampleCount == 1, "Resolve attachments must be single-sampled");

			auto texture = std::make_shared<VulkanTexture>(
				m_RenderContext,
				attachmentSpec.Format,
				m_Spec.Width, m_Spec.Height,
				finalLayout,
				usage,
				VK_SAMPLE_COUNT_1_BIT);

			m_ResolveImages.push_back(texture);
		}
	}
}

void VulkanRenderGraph::CreateRenderPass()
{
	std::vector<VkAttachmentDescription> attachments;

	std::vector<VkAttachmentReference> colorRefs;
	std::vector<VkAttachmentReference> resolveRefs;
	VkAttachmentReference depthRef{};

	std::vector<uint32_t> resolveAttachmentIndices;

	bool hasDepth = false;

	uint32_t attachmentIndex = 0;

	for (const auto& spec : m_Spec.Attachments)
	{
		VkAttachmentDescription desc{};
		desc.flags = 0;
		desc.samples = GetVulkanSampleCount(spec.SampleCount);

		desc.loadOp = spec.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR
			: VK_ATTACHMENT_LOAD_OP_LOAD;

		desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		desc.stencilLoadOp = desc.loadOp;
		desc.stencilStoreOp = desc.storeOp;

		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		if (spec.Type == AttachmentType::Color)
		{
			desc.format = TextureFormatToVkFormat(spec.Format, m_RenderContext);
			desc.finalLayout = spec.Sampled
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (spec.IsSwapChainAttachment)
				desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			if (spec.Texture)
				desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			attachments.push_back(desc);

			VkAttachmentReference ref{};
			ref.attachment = attachmentIndex;
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			colorRefs.push_back(ref);
		}
		else if (spec.Type == AttachmentType::Depth)
		{
			desc.format = TextureFormatToVkFormat(spec.Format, m_RenderContext);
			desc.finalLayout = spec.Sampled
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			if (spec.Texture)
				desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			attachments.push_back(desc);

			depthRef.attachment = attachmentIndex;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			hasDepth = true;
		}
		else if (spec.Type == AttachmentType::DepthStencil)
		{
			desc.format = TextureFormatToVkFormat(spec.Format, m_RenderContext);
			desc.finalLayout = spec.Sampled
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			if (spec.Texture)
				desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			attachments.push_back(desc);

			depthRef.attachment = attachmentIndex;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			hasDepth = true;
		}
		else if (spec.Type == AttachmentType::Resolve)
		{
			desc.format = TextureFormatToVkFormat(spec.Format, m_RenderContext);
			desc.samples = VK_SAMPLE_COUNT_1_BIT;
			desc.finalLayout = spec.Sampled
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (spec.IsSwapChainAttachment)
				desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			if (spec.Texture)
				desc.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			attachments.push_back(desc);
			resolveAttachmentIndices.push_back(attachmentIndex);
		}

		attachmentIndex++;
	}

	if (m_SampleCount > 1)
	{
		for (uint32_t i = 0; i < colorRefs.size(); i++)
		{
			VkAttachmentReference ref{};
			ref.attachment = resolveAttachmentIndices[i];
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			resolveRefs.push_back(ref);
		}
	}

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
	subpass.pColorAttachments = colorRefs.data();

	if (hasDepth)
		subpass.pDepthStencilAttachment = &depthRef;

	if (!resolveRefs.empty())
	{
		subpass.pResolveAttachments = resolveRefs.data();
	}

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask =
	VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
	VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	createInfo.pAttachments = attachments.data();
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &dependency;

	HY_ASSERT(vkCreateRenderPass(m_RenderContext->GetDevice(), &createInfo, nullptr, &m_RenderPass) == VK_SUCCESS, 
			 "Failed to create vulkan render pass");

	m_NumColorAttachments = static_cast<uint32_t>(colorRefs.size());
}

void VulkanRenderGraph::CreateFramebuffers()
{
	m_IsSwapChainBacked = false;
	for (const auto& spec : m_Spec.Attachments)
	{
		if (spec.IsSwapChainAttachment)
		{
			m_IsSwapChainBacked = true;
			break;
		}
	}

	if (m_IsSwapChainBacked)
		CreateSwapChainFramebuffers();
	else
		CreateTextureFramebuffers();
}

void VulkanRenderGraph::CreateSwapChainFramebuffers()
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
		uint32_t colorImageIdx = 0;
		uint32_t resolveImageIdx = 0;

		for (const auto& spec : m_Spec.Attachments)
		{
			if (spec.IsSwapChainAttachment)
			{
				attachments.push_back(swapchainImageViews[i]);
			}
			else if (spec.Type == AttachmentType::Color)
			{
				if (spec.Texture)
				{
					attachments.push_back(Texture::Get<VulkanTexture>(spec.Texture)->GetImageView());
					continue;
				}
				attachments.push_back(m_ColorImages[colorImageIdx++]->GetImageView());
			}
			else if (spec.Type == AttachmentType::Depth || spec.Type == AttachmentType::DepthStencil)
			{
				if (spec.Texture)
				{
					attachments.push_back(Texture::Get<VulkanTexture>(spec.Texture)->GetImageView());
					continue;
				}
				attachments.push_back(m_DepthImage->GetImageView());
			}
			else if (spec.Type == AttachmentType::Resolve)
			{
				if (spec.Texture)
				{
					attachments.push_back(Texture::Get<VulkanTexture>(spec.Texture)->GetImageView());
					continue;
				}
				attachments.push_back(m_ResolveImages[resolveImageIdx++]->GetImageView());
			}
		}

		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();

		HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) == VK_SUCCESS,
			"Failed to create vulkan swapchain framebuffer");
	}
}

void VulkanRenderGraph::CreateTextureFramebuffers()
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
		uint32_t colorImageIdx = 0;
		uint32_t resolveImageIdx = 0;

		for (const auto& spec : m_Spec.Attachments)
		{
			if (spec.Type == AttachmentType::Color)
			{
				if (spec.Texture)
				{
					attachmentViews.push_back(Texture::Get<VulkanTexture>(spec.Texture)->GetImageView());
					continue;
				}
				attachmentViews.push_back(m_ColorImages[colorImageIdx++]->GetImageView());
			}
			if (spec.Type == AttachmentType::Depth || spec.Type == AttachmentType::DepthStencil)
			{
				if (spec.Texture)
				{
					attachmentViews.push_back(Texture::Get<VulkanTexture>(spec.Texture)->GetImageView());
					continue;
				}
				attachmentViews.push_back(m_DepthImage->GetImageView());
			}
			if (spec.Type == AttachmentType::Resolve)
			{
				if (spec.Texture)
				{
					attachmentViews.push_back(Texture::Get<VulkanTexture>(spec.Texture)->GetImageView());
					continue;
				}
				attachmentViews.push_back(m_ResolveImages[resolveImageIdx++]->GetImageView());
			}
		}

		framebufferInfo.attachmentCount = attachmentViews.size();
		framebufferInfo.pAttachments = attachmentViews.data();

		HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_Framebuffers[i]) == VK_SUCCESS,
			"Failed to create vulkan framebuffer");
	}
}

void VulkanRenderGraph::OnResize(uint32_t width, uint32_t height)
{
	m_Spec.Width = width;
	m_Spec.Height = height;
	Invalidate();
}

void VulkanRenderGraph::Invalidate()
{
	vkDeviceWaitIdle(m_RenderContext->GetDevice());

	for (auto framebuffer : m_Framebuffers)
		vkDestroyFramebuffer(m_RenderContext->GetDevice(), framebuffer, nullptr);
	m_Framebuffers.clear();

	DestroyAttachments();
	CreateAttachments();
	CreateFramebuffers();
}

void VulkanRenderGraph::DestroyAttachments()
{
	m_ColorImages.clear();
	m_ResolveImages.clear();
	m_DepthImage = nullptr;
}
