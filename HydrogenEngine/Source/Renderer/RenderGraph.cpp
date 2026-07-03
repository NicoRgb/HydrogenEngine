#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

static size_t HashTextureDesc(const RgTextureDesc& desc, VkImageUsageFlags usage)
{
	size_t seed = 0;
	HashCombine(seed, static_cast<size_t>(desc.Width));
	HashCombine(seed, static_cast<size_t>(desc.Height));
	HashCombine(seed, static_cast<size_t>(desc.Format));
	HashCombine(seed, static_cast<size_t>(usage));
	return seed;
}

void RgCommandList::BindPipeline(const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader, PipelineSpec spec)
{
	size_t hash = spec.Hash();
	if (m_PipelineCache.find(hash) == m_PipelineCache.end())
	{
		m_PipelineCache[hash] = std::make_unique<Pipeline>(m_Device, m_RenderPass, vertexShader, fragmentShader, spec);
	}

	vkCmdBindPipeline(m_CmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineCache[hash]->GetPipeline());
}

void RgCommandList::Draw(uint32_t vertexCount)
{
	vkCmdDraw(m_CmdBuf, vertexCount, 1, 0, 0);
}

RgTextureHandle RgPassBuilder::WriteColor(RgTextureHandle texture)
{
	if (texture.IsValid())
	{
		m_PassNode.Usages.push_back({ RgResourceUsage::Type::ColorWrite, texture.Id });
	}
	return texture;
}

RgTextureHandle RgPassBuilder::WriteDepth(RgTextureHandle texture)
{
	if (texture.IsValid())
	{
		m_PassNode.Usages.push_back({ RgResourceUsage::Type::DepthWrite, texture.Id });
	}
	return texture;
}

RgTextureHandle RgPassBuilder::ReadTexture(RgTextureHandle texture)
{
	if (texture.IsValid())
	{
		m_PassNode.Usages.push_back({ RgResourceUsage::Type::ShaderRead, texture.Id });
	}
	return texture;
}

RenderGraph::RenderGraph(RenderDevice* device)
	: m_Device(device), m_CommandList(device)
{
	m_TextureDescs.reserve(64);
	m_PhysicalViews.reserve(64);
	m_PassNodes.reserve(32);
	m_CompiledPasses.reserve(32);
}

RenderGraph::~RenderGraph()
{
	VkDevice device = m_Device->GetVulkanDevice();

	for (auto& [hash, rp] : m_RenderPassCache) vkDestroyRenderPass(device, rp, nullptr);
	for (auto& [hash, fb] : m_FramebufferCache) vkDestroyFramebuffer(device, fb, nullptr);

	for (auto& pooled : m_PhysicalTexturePool)
	{
		vkDestroyImageView(device, pooled.View, nullptr);
		vmaDestroyImage(m_Device->GetAllocator(), pooled.Image, pooled.Allocation);
	}
	m_PhysicalTexturePool.clear();
}

void RenderGraph::Reset()
{
	m_TextureDescs.clear();
	m_PhysicalViews.clear();
	m_PassNodes.clear();
	m_CompiledPasses.clear();
	m_PostRenderBarriers.clear();

	for (auto& pooled : m_PhysicalTexturePool)
	{
		pooled.IsFree = true;
	}
}

RgTextureHandle RenderGraph::CreateTexture(const RgTextureDesc& desc)
{
	uint32_t id = static_cast<uint32_t>(m_TextureDescs.size());
	m_TextureDescs.push_back(desc);

	RgResourceView view{};
	view.IsImported = false;
	m_PhysicalViews.push_back(view);

	return RgTextureHandle{ id };
}

RgTextureHandle RenderGraph::ImportTexture(VkImage image, VkImageView imageView, const RgTextureDesc& desc)
{
	uint32_t id = static_cast<uint32_t>(m_TextureDescs.size());
	m_TextureDescs.push_back(desc);

	RgResourceView view{};
	view.Image = image;
	view.ImageView = imageView;
	view.IsImported = true;
	m_PhysicalViews.push_back(view);

	return RgTextureHandle{ id };
}

void RenderGraph::AddPass(const std::string& passName, std::function<void(RgPassBuilder& builder)> setupFunc, std::function<void(RgCommandList& cmd)> executeFunc)
{
	RgPassNode newNode{};
	newNode.Name = passName;
	newNode.ExecuteCallback = executeFunc;

	RgPassBuilder builder(newNode);
	setupFunc(builder);
	
	m_PassNodes.push_back(builder.GetNode());
}

struct TextureStateTracker
{
	VkImageLayout CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineStageFlags AccessStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkAccessFlags AccessMask = 0;
};

void RenderGraph::Compile()
{
	for (const auto& pass : m_PassNodes)
	{
		for (const auto& usage : pass.Usages)
		{
			auto& view = m_PhysicalViews[usage.Handle.Id];
			switch (usage.UsageType)
			{
			case RgResourceUsage::Type::ShaderRead: view.UsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT; break;
			case RgResourceUsage::Type::ColorWrite: view.UsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; break;
			case RgResourceUsage::Type::DepthWrite: view.UsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; break;
			}
		}
	}

	for (size_t i = 0; i < m_PhysicalViews.size(); ++i)
	{
		if (!m_PhysicalViews[i].IsImported)
		{
			size_t descHash = HashTextureDesc(m_TextureDescs[i], m_PhysicalViews[i].UsageFlags);
			bool foundCachedResource = false;

			for (auto& pooled : m_PhysicalTexturePool)
			{
				if (pooled.IsFree && pooled.Hash == descHash)
				{
					m_PhysicalViews[i].Image = pooled.Image;
					m_PhysicalViews[i].ImageView = pooled.View;
					pooled.IsFree = false;
					foundCachedResource = true;
					break;
				}
			}

			if (!foundCachedResource)
			{
				VmaAllocation allocationHandle = VK_NULL_HANDLE;

				VkImage physicalImage = CreatePhysicalImage(m_TextureDescs[i], m_PhysicalViews[i].UsageFlags, &allocationHandle);
				VkImageView physicalView = CreatePhysicalImageView(physicalImage, m_TextureDescs[i], m_PhysicalViews[i].UsageFlags);

				m_PhysicalViews[i].Image = physicalImage;
				m_PhysicalViews[i].ImageView = physicalView;

				PooledTexture newPooled{};
				newPooled.Image = physicalImage;
				newPooled.View = physicalView;
				newPooled.Allocation = allocationHandle;
				newPooled.Hash = descHash;
				newPooled.IsFree = false;

				m_PhysicalTexturePool.push_back(newPooled);
			}
		}
	}

	std::vector<TextureStateTracker> textureStates(m_PhysicalViews.size());
	std::vector<bool> textureWrittenThisFrame(m_PhysicalViews.size(), false);
	std::vector<RenderPassAttachment> passColorAttachments;

	for (const auto& recordedPass : m_PassNodes)
	{
		CompiledPass compiledPass{};
		compiledPass.Name = recordedPass.Name;
		compiledPass.ExecuteCallback = recordedPass.ExecuteCallback;
		passColorAttachments.clear();

		std::optional<RenderPassAttachment> passDepthAttachment = std::nullopt;
		bool extentSet = false;

		for (const auto& usage : recordedPass.Usages)
		{
			const auto& view = m_PhysicalViews[usage.Handle.Id];
			const auto& desc = m_TextureDescs[usage.Handle.Id];
			auto& state = textureStates[usage.Handle.Id];

			if (!extentSet)
			{
				compiledPass.RenderExtent = { desc.Width, desc.Height };
				extentSet = true;
			}

			VkImageLayout targetLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			VkAccessFlags targetAccess = 0;
			VkPipelineStageFlags targetStage = 0;
			VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			bool isFirstWrite = !textureWrittenThisFrame[usage.Handle.Id];
			VkAttachmentLoadOp currentLoadOp = isFirstWrite ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;

			switch (usage.UsageType)
			{
			case RgResourceUsage::Type::ColorWrite:
			{
				targetLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
				targetAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
				targetStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

				VkFormat vkFormat = VK_FORMAT_R8G8B8A8_SRGB;

				switch (desc.Format)
				{
				case TextureFormat::RGBA8_SRGB:    vkFormat = VK_FORMAT_R8G8B8A8_SRGB; break;
				case TextureFormat::RGBA16_SFLOAT: vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT; break;
				case TextureFormat::BGRA8_SRGB:    vkFormat = VK_FORMAT_B8G8R8A8_SRGB; break;
				case TextureFormat::D32_SFLOAT:
					vkFormat = VK_FORMAT_D32_SFLOAT;
					aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
					break;
				}

				textureWrittenThisFrame[usage.Handle.Id] = true;

				RenderPassAttachment att{};
				att.Format = vkFormat;
				att.InitialLayout = state.CurrentLayout;
				att.LoadOp = currentLoadOp;
				passColorAttachments.push_back(att);

				VkClearValue clear{};
				clear.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
				compiledPass.ClearValues.push_back(clear);

				compiledPass.ColorFormats.push_back(vkFormat);
				compiledPass.ColorAttachments.push_back(view.ImageView);
				break;
			}

			case RgResourceUsage::Type::DepthWrite:
			{
				targetLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
				targetAccess = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				targetStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
				aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

				textureWrittenThisFrame[usage.Handle.Id] = true;

				RenderPassAttachment att{};
				att.Format = VK_FORMAT_D32_SFLOAT;
				att.InitialLayout = state.CurrentLayout;
				att.LoadOp = currentLoadOp;
				passDepthAttachment = att;

				compiledPass.DepthFormat = VK_FORMAT_D32_SFLOAT;
				compiledPass.DepthAttachment = view.ImageView;
				break;
			}

			case RgResourceUsage::Type::ShaderRead:
				targetLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				targetAccess = VK_ACCESS_SHADER_READ_BIT;
				targetStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
				if (desc.Format == TextureFormat::D32_SFLOAT) aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				break;
			}

			if (state.CurrentLayout != targetLayout)
			{
				VkBarrierCommand cmd{};
				cmd.TextureId = usage.Handle.Id;
				cmd.SrcStage = state.AccessStage;
				cmd.DstStage = targetStage;

				cmd.Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				cmd.Barrier.pNext = nullptr;
				cmd.Barrier.oldLayout = state.CurrentLayout;
				cmd.Barrier.newLayout = targetLayout;
				cmd.Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				cmd.Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				cmd.Barrier.image = view.Image;

				cmd.Barrier.subresourceRange.aspectMask = aspectMask;
				cmd.Barrier.subresourceRange.baseMipLevel = 0;
				cmd.Barrier.subresourceRange.levelCount = 1;
				cmd.Barrier.subresourceRange.baseArrayLayer = 0;
				cmd.Barrier.subresourceRange.layerCount = 1;

				cmd.Barrier.srcAccessMask = state.AccessMask;
				cmd.Barrier.dstAccessMask = targetAccess;

				compiledPass.PrePassBarriers.push_back(cmd);

				state.CurrentLayout = targetLayout;
				state.AccessStage = targetStage;
				state.AccessMask = targetAccess;
			}
		}

		if (passDepthAttachment.has_value())
		{
			VkClearValue depthClear{};
			depthClear.depthStencil = { 1.0f, 0 };
			compiledPass.ClearValues.push_back(depthClear);
		}

		compiledPass.RenderPass = GetOrCreateRenderPass(passColorAttachments, passDepthAttachment);
		compiledPass.Framebuffer = GetOrCreateFramebuffer(compiledPass.RenderPass, compiledPass.ColorAttachments, compiledPass.DepthAttachment, compiledPass.RenderExtent);

		m_CompiledPasses.push_back(std::move(compiledPass));
	}

	if (!m_CompiledPasses.empty())
	{
		for (size_t i = 0; i < m_PhysicalViews.size(); i++)
		{
			auto& view = m_PhysicalViews[i];
			auto& state = textureStates[i];

			if (view.IsImported && state.CurrentLayout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
			{
				VkBarrierCommand finalCmd{};
				finalCmd.TextureId = i;
				finalCmd.SrcStage = state.AccessStage;
				finalCmd.DstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

				finalCmd.Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				finalCmd.Barrier.oldLayout = state.CurrentLayout;
				finalCmd.Barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
				finalCmd.Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				finalCmd.Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				finalCmd.Barrier.image = view.Image;

				finalCmd.Barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				finalCmd.Barrier.subresourceRange.baseMipLevel = 0;
				finalCmd.Barrier.subresourceRange.levelCount = 1;
				finalCmd.Barrier.subresourceRange.baseArrayLayer = 0;
				finalCmd.Barrier.subresourceRange.layerCount = 1;

				finalCmd.Barrier.srcAccessMask = state.AccessMask;
				finalCmd.Barrier.dstAccessMask = 0;

				m_PostRenderBarriers.push_back(finalCmd);
				state.CurrentLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			}
		}
	}
}

void RenderGraph::Execute(VkCommandBuffer cmdBuffer)
{
	m_CommandList.InitFrame(cmdBuffer, &m_PhysicalViews);

	for (const auto& pass : m_CompiledPasses)
	{
		for (const auto& barrierCmd : pass.PrePassBarriers)
		{
			vkCmdPipelineBarrier(cmdBuffer, barrierCmd.SrcStage, barrierCmd.DstStage, 0,
				0, nullptr, 0, nullptr, 1, &barrierCmd.Barrier);
		}

		VkRenderPassBeginInfo beginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		beginInfo.renderPass = pass.RenderPass;
		beginInfo.framebuffer = pass.Framebuffer;
		beginInfo.renderArea.extent = pass.RenderExtent;

		beginInfo.clearValueCount = static_cast<uint32_t>(pass.ClearValues.size());
		beginInfo.pClearValues = pass.ClearValues.empty() ? nullptr : pass.ClearValues.data();

		vkCmdBeginRenderPass(cmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(pass.RenderExtent.width);
		viewport.height = static_cast<float>(pass.RenderExtent.height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = pass.RenderExtent;
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

		m_CommandList.InitPass(pass.RenderPass);

		pass.ExecuteCallback(m_CommandList);

		vkCmdEndRenderPass(cmdBuffer);
	}
	
	for (const auto& barrierCmd : m_PostRenderBarriers)
	{
		vkCmdPipelineBarrier(
			cmdBuffer,
			barrierCmd.SrcStage,
			barrierCmd.DstStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrierCmd.Barrier
		);
	}
}

VkRenderPass RenderGraph::GetOrCreateRenderPass(const std::vector<RenderPassAttachment>& colors, std::optional<RenderPassAttachment> depth)
{
	size_t hash = 0;
	if (depth.has_value())
	{
		HashCombine(hash, static_cast<size_t>(depth.value().Format));
		HashCombine(hash, static_cast<size_t>(depth.value().InitialLayout));
		HashCombine(hash, static_cast<size_t>(depth.value().LoadOp));
	}
	HashCombine(hash, colors.size());
	for (const auto& color : colors)
	{
		HashCombine(hash, static_cast<size_t>(color.Format));
		HashCombine(hash, static_cast<size_t>(color.InitialLayout));
		HashCombine(hash, static_cast<size_t>(color.LoadOp));
	}

	if (m_RenderPassCache.count(hash))
	{
		return m_RenderPassCache.at(hash);
	}

	std::vector<VkAttachmentDescription> attachments;
	std::vector<VkAttachmentReference> colorReferences;
	VkAttachmentReference depthReference{};

	for (uint32_t i = 0; i < colors.size(); i++)
	{
		VkAttachmentDescription desc{};
		desc.format = colors[i].Format;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.loadOp = colors[i].LoadOp;
		desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.initialLayout = colors[i].InitialLayout;
		desc.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		colorReferences.push_back({ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		attachments.push_back(desc);
	}

	if (depth.has_value())
	{
		VkAttachmentDescription desc{};
		desc.format = depth.value().Format;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.loadOp = depth.value().LoadOp;
		desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		desc.initialLayout = depth.value().InitialLayout;
		desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		depthReference.attachment = (uint32_t)colors.size();
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		attachments.push_back(desc);
	}

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
	subpass.pColorAttachments = colorReferences.empty() ? nullptr : colorReferences.data();
	subpass.pDepthStencilAttachment = depth.has_value() ? &depthReference : nullptr;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	if (depth.has_value())
	{
		dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	if (depth.has_value())
	{
		dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	}
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	if (depth.has_value())
	{
		dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VkRenderPass renderPass;
	VkResult result = vkCreateRenderPass(m_Device->GetVulkanDevice(), &renderPassInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan render pass... vkCreateRenderPass returned {}", (uint16_t)result);
	}

	m_RenderPassCache[hash] = renderPass;
	return renderPass;
}

VkFramebuffer RenderGraph::GetOrCreateFramebuffer(VkRenderPass rp, const std::vector<VkImageView>& views, VkImageView depthView, VkExtent2D extent)
{
	size_t hash = 0;
	HashCombine(hash, reinterpret_cast<size_t>(rp));
	HashCombine(hash, reinterpret_cast<size_t>(depthView));
	HashCombine(hash, static_cast<size_t>(extent.width));
	HashCombine(hash, static_cast<size_t>(extent.height));
	HashCombine(hash, views.size());
	for (auto attachment : views)
	{
		HashCombine(hash, reinterpret_cast<size_t>(attachment));
	}

	if (m_FramebufferCache.count(hash))
	{
		return m_FramebufferCache.at(hash);
	}

	std::vector<VkImageView> attachments = views;
	if (depthView != VK_NULL_HANDLE)
	{
		attachments.push_back(depthView);
	}

	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = rp;
	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = extent.width;
	framebufferInfo.height = extent.height;
	framebufferInfo.layers = 1;

	VkFramebuffer framebuffer;
	VkResult result = vkCreateFramebuffer(m_Device->GetVulkanDevice(), &framebufferInfo, nullptr, &framebuffer);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan framebuffer... vkCreateFramebuffer returned {}", (uint16_t)result);
	}

	m_FramebufferCache[hash] = framebuffer;
	return framebuffer;
}

VkImage RenderGraph::CreatePhysicalImage(const RgTextureDesc& desc, VkImageUsageFlags usage, VmaAllocation* outAllocation)
{
	VkFormat vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
	switch (desc.Format)
	{
	case TextureFormat::RGBA8_SRGB:    vkFormat = VK_FORMAT_R8G8B8A8_SRGB; break;
	case TextureFormat::RGBA16_SFLOAT: vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT; break;
	case TextureFormat::BGRA8_SRGB:    vkFormat = VK_FORMAT_B8G8R8A8_SRGB; break;
	case TextureFormat::D32_SFLOAT:    vkFormat = VK_FORMAT_D32_SFLOAT; break;
	}

	VkImageCreateInfo imageInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = desc.Width;
	imageInfo.extent.height = desc.Height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = vkFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

	VkImage image = VK_NULL_HANDLE;
	VkResult result = vmaCreateImage(
		m_Device->GetAllocator(),
		&imageInfo,
		&allocInfo,
		&image,
		outAllocation,
		nullptr
	);

	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("VMA failed to allocate and create transient physical VkImage.");
	}

	return image;
}

VkImageView RenderGraph::CreatePhysicalImageView(VkImage image, const RgTextureDesc& desc, VkImageUsageFlags usage)
{
	VkFormat vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
	VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	switch (desc.Format)
	{
	case TextureFormat::RGBA8_SRGB:    vkFormat = VK_FORMAT_R8G8B8A8_SRGB; break;
	case TextureFormat::RGBA16_SFLOAT: vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT; break;
	case TextureFormat::BGRA8_SRGB:    vkFormat = VK_FORMAT_B8G8R8A8_SRGB; break;
	case TextureFormat::D32_SFLOAT:
		vkFormat = VK_FORMAT_D32_SFLOAT;
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	}

	VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = vkFormat;

	viewInfo.subresourceRange.aspectMask = aspectMask;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	VkImageView imageView = VK_NULL_HANDLE;
	VkResult result = vkCreateImageView(m_Device->GetVulkanDevice(), &viewInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("RenderGraph failed to create physical VkImageView structure.");
	}

	return imageView;
}

uint32_t RenderGraph::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_Device->GetVulkanPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	HY_ENGINE_FATAL("Failed to find suitable memory type for transient graph texture!");
	return 0;
}
