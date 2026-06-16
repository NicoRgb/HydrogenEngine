#include "Hydrogen/Platform/Vulkan/VulkanRenderGraph.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include "Hydrogen/Renderer/Pipeline.hpp"
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

VulkanFrameGraph::VulkanFrameGraph(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)),
	m_Width(width), m_Height(height)
{
}

VulkanFrameGraph::~VulkanFrameGraph()
{
}

void VulkanFrameGraph::Compose()
{
	for (const auto& [name, pass] : m_Passes)
	{
		CreatePass(pass.get(), name);
	}
}

void VulkanFrameGraph::Resize(uint32_t width, uint32_t height)
{
	m_Width = width;
	m_Height = height;

	m_Attachments.clear();
	m_VkFramePasses.clear();

	Compose();
}

void VulkanFrameGraph::CreatePass(FramePass* pass, std::string name)
{
	m_VkFramePasses[name] = {};

	for (auto& [n, attachment] : pass->GetResources())
	{
		if (attachment.IsSwapChainAttachment)
		{
			m_VkFramePasses[name].IsSwapchainBacked = true;
		}

		if (m_Attachments.find(n) == m_Attachments.end())
		{	
			CreateAttachment(n, attachment);
		}
	}

	CreateRenderPass(name, pass);
	if (m_VkFramePasses[name].IsSwapchainBacked)
		CreateFramebuffersSwapchainBacked(name, pass);
	else
		CreateFramebuffers(name, pass);
	CreatePipelines(name, pass);
}

void VulkanFrameGraph::CreateAttachment(std::string name, const FrameAttachmentHandle& handle)
{
	if (handle.Type == FrameAttachmentType::Color)
	{
		if (handle.IsSwapChainAttachment)
		{
			HY_ASSERT(handle.SampleCount == 1, "Swap chain attachments must be single-sampled");
			HY_ASSERT(handle.Format == TextureFormat::ViewportDefault, "Swap chain attachments must use the viewport default format");
			m_Attachments[name] = { NULL, VK_IMAGE_LAYOUT_UNDEFINED };

			return;
		}

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if (handle.Sampled)
		{
			usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		auto texture = std::make_shared<VulkanTexture>(
			m_RenderContext,
			handle.Format,
			m_Width, m_Height,
			finalLayout,
			usage,
			GetVulkanSampleCount(handle.SampleCount));

		m_Attachments[name] = { texture, VK_IMAGE_LAYOUT_UNDEFINED };
	}
	else if (handle.Type == FrameAttachmentType::Depth)
	{
		HY_ASSERT(handle.IsSwapChainAttachment == false, "Depth attachments cannot be swap chain attachments");

		VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (handle.Sampled)
		{
			usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
			finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		auto texture = std::make_shared<VulkanTexture>(
			m_RenderContext,
			handle.Format,
			m_Width, m_Height,
			finalLayout,
			usage,
			GetVulkanSampleCount(handle.SampleCount));

		m_Attachments[name] = { texture, VK_IMAGE_LAYOUT_UNDEFINED };
	}
	else if (handle.Type == FrameAttachmentType::DepthStencil)
	{
		HY_ASSERT(handle.IsSwapChainAttachment == false, "Depth-stencil attachments cannot be swap chain attachments");

		VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		if (handle.Sampled)
		{
			usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
			finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		auto texture = std::make_shared<VulkanTexture>(
			m_RenderContext,
			handle.Format,
			m_Width, m_Height,
			finalLayout,
			usage,
			GetVulkanSampleCount(handle.SampleCount));

		m_Attachments[name] = { texture, VK_IMAGE_LAYOUT_UNDEFINED };
	}
	else if (handle.Type == FrameAttachmentType::Resolve)
	{
		if (handle.IsSwapChainAttachment)
		{
			HY_ASSERT(handle.Format == TextureFormat::ViewportDefault, "Swap chain attachments must use the viewport default format");
			m_Attachments[name] = { NULL, VK_IMAGE_LAYOUT_UNDEFINED };

			return;
		}

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		if (handle.Sampled)
		{
			usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		HY_ASSERT(handle.SampleCount == 1, "Resolve attachments must be single-sampled");

		auto texture = std::make_shared<VulkanTexture>(
			m_RenderContext,
			handle.Format,
			m_Width, m_Height,
			finalLayout,
			usage,
			VK_SAMPLE_COUNT_1_BIT);

		m_Attachments[name] = { texture, VK_IMAGE_LAYOUT_UNDEFINED };
	}
}

void VulkanFrameGraph::CreateRenderPass(std::string name, FramePass* pass)
{
	std::vector<VkAttachmentDescription> attachments;

	std::vector<VkAttachmentReference> colorRefs;
	std::vector<VkAttachmentReference> resolveRefs;
	VkAttachmentReference depthRef{};

	std::vector<uint32_t> resolveAttachmentIndices;

	bool hasDepth = false;
	bool depthCleared = false;

	uint32_t attachmentIndex = 0;
	uint32_t sampleCount = 1;

	uint32_t numColorAttachments = 0;
	uint32_t numClearedColorAttachments = 0;

	for (const auto& name : pass->GetResourceOrder())
	{
		FrameAttachmentHandle& attachmentHandle = pass->GetResources().at(name);
		FrameAttachment& attachment = m_Attachments[name];

		VkAttachmentDescription desc{};
		desc.flags = 0;
		desc.samples = GetVulkanSampleCount(attachmentHandle.SampleCount);
		desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

		if (!attachment.Cleared && attachmentHandle.Cleared)
		{
			attachment.Cleared = true;
			desc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

			if (attachmentHandle.Type == FrameAttachmentType::Color)
				numClearedColorAttachments++;
			else if (attachmentHandle.Type == FrameAttachmentType::Depth || attachmentHandle.Type == FrameAttachmentType::DepthStencil)
				depthCleared = true;
		}

		desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		desc.stencilLoadOp = desc.loadOp;
		desc.stencilStoreOp = desc.storeOp;

		desc.initialLayout = attachment.CurrentLayout;

		if (attachmentHandle.Type == FrameAttachmentType::Color)
		{
			numColorAttachments++;
			sampleCount = attachmentHandle.SampleCount;

			desc.format = TextureFormatToVkFormat(attachmentHandle.Format, m_RenderContext);
			desc.finalLayout = attachmentHandle.Sampled
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (attachmentHandle.IsSwapChainAttachment)
				desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			attachments.push_back(desc);

			VkAttachmentReference ref{};
			ref.attachment = attachmentIndex;
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			colorRefs.push_back(ref);
		}
		else if (attachmentHandle.Type == FrameAttachmentType::Depth)
		{
			desc.format = TextureFormatToVkFormat(attachmentHandle.Format, m_RenderContext);
			desc.finalLayout = attachmentHandle.Sampled
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			attachments.push_back(desc);

			depthRef.attachment = attachmentIndex;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			hasDepth = true;
		}
		else if (attachmentHandle.Type == FrameAttachmentType::DepthStencil)
		{
			desc.format = TextureFormatToVkFormat(attachmentHandle.Format, m_RenderContext);
			desc.finalLayout = attachmentHandle.Sampled
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			attachments.push_back(desc);

			depthRef.attachment = attachmentIndex;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			hasDepth = true;
		}
		else if (attachmentHandle.Type == FrameAttachmentType::Resolve)
		{
			desc.format = TextureFormatToVkFormat(attachmentHandle.Format, m_RenderContext);
			desc.samples = VK_SAMPLE_COUNT_1_BIT;
			desc.finalLayout = attachmentHandle.Sampled
				? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				: VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			if (attachmentHandle.IsSwapChainAttachment)
				desc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			attachments.push_back(desc);
			resolveAttachmentIndices.push_back(attachmentIndex);
		}

		attachment.CurrentLayout = desc.finalLayout;
		attachmentIndex++;
	}


	if (sampleCount > 1)
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
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	createInfo.pAttachments = attachments.data();
	createInfo.subpassCount = 1;
	createInfo.pSubpasses = &subpass;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &dependency;

	HY_ASSERT(vkCreateRenderPass(m_RenderContext->GetDevice(), &createInfo, nullptr, &m_VkFramePasses.at(name).RenderPass) == VK_SUCCESS,
		"Failed to create vulkan render pass");

	m_VkFramePasses.at(name).HasDepthAttachment = hasDepth;
	m_VkFramePasses.at(name).DepthCleared = depthCleared;
	m_VkFramePasses.at(name).NumColorAttachments = numColorAttachments;
	m_VkFramePasses.at(name).NumClearedColorAttachments = numClearedColorAttachments;
	m_VkFramePasses.at(name).SampleCount = sampleCount;
}

void VulkanFrameGraph::CreateFramebuffers(std::string name, FramePass* pass)
{
	m_VkFramePasses.at(name).Framebuffers.resize(m_RenderContext->GetMaxFramesInFlight());

	for (size_t i = 0; i < m_RenderContext->GetMaxFramesInFlight(); i++)
	{
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_VkFramePasses.at(name).RenderPass;
		framebufferInfo.width = m_Width;
		framebufferInfo.height = m_Height;
		framebufferInfo.layers = 1;

		std::vector<VkImageView> attachmentViews;

		for (auto& [n, attachmentHandle] : pass->GetResources())
		{
			FrameAttachment& attachment = m_Attachments[n];

			if (attachmentHandle.Type == FrameAttachmentType::Color)
			{
				attachmentViews.push_back(attachment.Texture->GetImageView());
			}
			else if (attachmentHandle.Type == FrameAttachmentType::Depth || attachmentHandle.Type == FrameAttachmentType::DepthStencil)
			{
				attachmentViews.push_back(attachment.Texture->GetImageView());
			}
			else if (attachmentHandle.Type == FrameAttachmentType::Resolve)
			{
				attachmentViews.push_back(attachment.Texture->GetImageView());
			}
		}

		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
		framebufferInfo.pAttachments = attachmentViews.data();

		HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_VkFramePasses.at(name).Framebuffers[i]) == VK_SUCCESS,
			"Failed to create vulkan framebuffer");
	}
}

void VulkanFrameGraph::CreateFramebuffersSwapchainBacked(std::string name, FramePass* pass)
{
	m_VkFramePasses.at(name).Framebuffers.resize(m_RenderContext->GetSwapChainImages().size());

	for (size_t i = 0; i < m_RenderContext->GetSwapChainImages().size(); i++)
	{
		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_VkFramePasses.at(name).RenderPass;
		framebufferInfo.width = m_Width;
		framebufferInfo.height = m_Height;
		framebufferInfo.layers = 1;

		std::vector<VkImageView> attachmentViews;

		for (auto& [n, attachmentHandle] : pass->GetResources())
		{
			FrameAttachment& attachment = m_Attachments[n];

			if (attachmentHandle.Type == FrameAttachmentType::Color && attachmentHandle.IsSwapChainAttachment)
			{
				attachmentViews.push_back(m_RenderContext->GetSwapChainImageViews()[i]);
			}
			else if (attachmentHandle.Type == FrameAttachmentType::Color)
			{
				attachmentViews.push_back(attachment.Texture->GetImageView());
			}
			else if (attachmentHandle.Type == FrameAttachmentType::Depth || attachmentHandle.Type == FrameAttachmentType::DepthStencil)
			{
				attachmentViews.push_back(attachment.Texture->GetImageView());
			}
			else if (attachmentHandle.Type == FrameAttachmentType::Resolve && attachmentHandle.IsSwapChainAttachment)
			{
				attachmentViews.push_back(m_RenderContext->GetSwapChainImageViews()[i]);
			}
			else if (attachmentHandle.Type == FrameAttachmentType::Resolve)
			{
				attachmentViews.push_back(attachment.Texture->GetImageView());
			}
		}

		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
		framebufferInfo.pAttachments = attachmentViews.data();

		HY_ASSERT(vkCreateFramebuffer(m_RenderContext->GetDevice(), &framebufferInfo, nullptr, &m_VkFramePasses.at(name).Framebuffers[i]) == VK_SUCCESS,
			"Failed to create vulkan framebuffer");
	}
}

void VulkanFrameGraph::CreatePipelines(std::string name, FramePass* pass)
{
	auto& pipelineSpecs = pass->GetPipelineSpecs();
	auto& pipelines = pass->GetPipelines();

	for (auto& [pipelineName, spec] : pipelineSpecs)
	{
		if (pipelines.find(pipelineName) != pipelines.end())
			continue;

		auto pipeline = Pipeline::Create(m_RenderContext, shared_from_this(), name, spec);
		pipelines[pipelineName] = pipeline;
	}
}
