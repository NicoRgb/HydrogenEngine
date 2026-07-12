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

static size_t HashDescriptorBindings(const std::vector<DescriptorBinding>& bindings)
{
	size_t seed = 0;
	for (const auto& binding : bindings)
	{
		HashCombine(seed, binding.Binding);
		HashCombine(seed, static_cast<size_t>(binding.Type));
	}
	return seed;
}

void RgCommandList::PushConstants(const void* data, uint32_t size, uint32_t offset, ShaderStage stageFlags)
{
	HY_ASSERT(m_BoundPipeline, "No pipeline bound in PushConstants");

	VkShaderStageFlags vkStageFlags = 0;
	if (((uint32_t)stageFlags & (uint32_t)ShaderStage::Vertex) == (uint32_t)ShaderStage::Vertex)
	{
		vkStageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
	}
	if (((uint32_t)stageFlags & (uint32_t)ShaderStage::Fragment) == (uint32_t)ShaderStage::Fragment)
	{
		vkStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
	}

	vkCmdPushConstants(m_CmdBuf, m_BoundPipeline->GetPipelineLayout(), vkStageFlags, offset, size, data);
}

void RgCommandList::UpdateDescriptorSet(const std::vector<DescriptorBindingValue>& bindingValues)
{
	for (uint32_t i = 0; i < bindingValues.size(); i++)
	{
		switch (bindingValues[i].Type)
		{
		case DescriptorType::UniformBuffer:
			for (uint32_t j = 0; j < bindingValues[i].RenderBuffers.size(); j++)
			{
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = bindingValues[i].RenderBuffers[j]->GetBuffer();
				bufferInfo.offset = 0;
				bufferInfo.range = bindingValues[i].RenderBuffers[j]->GetSize();

				VkWriteDescriptorSet descriptorWrite{};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = m_PassDescriptorSet;
				descriptorWrite.dstBinding = i;
				descriptorWrite.dstArrayElement = j;

				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrite.descriptorCount = 1;

				descriptorWrite.pBufferInfo = &bufferInfo;
				descriptorWrite.pImageInfo = nullptr;

				vkUpdateDescriptorSets(m_Device->GetVulkanDevice(), 1, &descriptorWrite, 0, nullptr);
			}
			break;

		case DescriptorType::StorageBuffer:
			HY_ASSERT(false, "Not implemented");
			break;

		case DescriptorType::CombinedImageSampler:
			HY_ASSERT(false, "Not implemented");
			break;
		}
	}
}

void RgCommandList::BindPipeline(const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader, PipelineSpec spec)
{
	spec.DescriptorSetLayouts = m_DescriptorSetLayouts;

	size_t hash = spec.Hash();
	HashCombine(hash, std::hash<std::string>{}(vertexShader->GetContent()));
	HashCombine(hash, std::hash<std::string>{}(fragmentShader->GetContent()));
	HashCombine(hash, reinterpret_cast<size_t>(m_RenderPass));

	if (m_PipelineCache.find(hash) == m_PipelineCache.end())
	{
		m_PipelineCache[hash] = std::make_unique<Pipeline>(m_Device, m_RenderPass, vertexShader, fragmentShader, spec);
	}
	
	std::vector<VkDescriptorSet> sets;
	if (m_FrameDescriptorSet != VK_NULL_HANDLE)
	{
		sets.push_back(m_FrameDescriptorSet);
	}
	if (m_PassDescriptorSet != VK_NULL_HANDLE)
	{
		sets.push_back(m_PassDescriptorSet);
	}

	if (sets.size() > 0)
	{
		vkCmdBindDescriptorSets(m_CmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineCache[hash]->GetPipelineLayout(), 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);
	}
	
	vkCmdBindPipeline(m_CmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineCache[hash]->GetPipeline());

	m_BoundPipeline = m_PipelineCache.at(hash).get();
}

void RgCommandList::BindVertexBuffer(const RenderBuffer* vertexBuffer)
{
	VkBuffer vertexBuffers[] = { vertexBuffer->GetBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(m_CmdBuf, 0, 1, vertexBuffers, offsets);
}

void RgCommandList::BindIndexBuffer(const RenderBuffer* indexBuffer)
{
	vkCmdBindIndexBuffer(m_CmdBuf, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);
}

void RgCommandList::Draw(uint32_t vertexCount)
{
	vkCmdDraw(m_CmdBuf, vertexCount, 1, 0, 0);
}

void Hydrogen::RgCommandList::DrawIndexed(uint32_t indexCount)
{
	vkCmdDrawIndexed(m_CmdBuf, indexCount, 1, 0, 0, 0);
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

	VkDescriptorPoolSize poolSizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
	poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
	poolInfo.pPoolSizes = poolSizes;

	VkResult result = vkCreateDescriptorPool(m_Device->GetVulkanDevice(), &poolInfo, nullptr, &m_DescriptorPool);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan descriptor pool... vkCreateDescriptorPool returned {}", (uint16_t)result);
	}
}

RenderGraph::~RenderGraph()
{
	VkDevice device = m_Device->GetVulkanDevice();

	for (auto& [hash, rp] : m_RenderPassCache) vkDestroyRenderPass(device, rp, nullptr);
	for (auto& [hash, fb] : m_FramebufferCache) vkDestroyFramebuffer(device, fb, nullptr);

	for (auto& pooled : m_DescriptorSetPool)
	{
		vkDestroyDescriptorSetLayout(device, pooled.DescriptorSetLayout, nullptr);
	}

	for (auto& pooled : m_PhysicalTexturePool)
	{
		vkDestroyImageView(device, pooled.View, nullptr);
		vmaDestroyImage(m_Device->GetAllocator(), pooled.Image, pooled.Allocation);
	}
	m_PhysicalTexturePool.clear();

	vkDestroyDescriptorPool(device, m_DescriptorPool, nullptr);
}

void RenderGraph::Reset()
{
	m_TextureDescs.clear();
	m_PhysicalViews.clear();
	m_PassNodes.clear();
	m_CompiledPasses.clear();
	m_PostRenderBarriers.clear();

	for (auto& pooled : m_DescriptorSetPool)
	{
		pooled.IsFree = true;
	}

	for (auto& pooled : m_PhysicalTexturePool)
	{
		pooled.IsFree = true;
	}
}

void RenderGraph::ClearCache()
{
	Reset();

	VkDevice device = m_Device->GetVulkanDevice();

	for (auto& [hash, rp] : m_RenderPassCache) vkDestroyRenderPass(device, rp, nullptr);
	for (auto& [hash, fb] : m_FramebufferCache) vkDestroyFramebuffer(device, fb, nullptr);

	for (auto& pooled : m_DescriptorSetPool)
	{
		vkFreeDescriptorSets(device, m_DescriptorPool, 1, &pooled.DescriptorSet);
		vkDestroyDescriptorSetLayout(device, pooled.DescriptorSetLayout, nullptr);
	}

	for (auto& pooled : m_PhysicalTexturePool)
	{
		vkDestroyImageView(device, pooled.View, nullptr);
		vmaDestroyImage(m_Device->GetAllocator(), pooled.Image, pooled.Allocation);
	}

	m_RenderPassCache.clear();
	m_FramebufferCache.clear();
	m_DescriptorSetPool.clear();
	m_PhysicalTexturePool.clear();
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

void RenderGraph::AddPass(const std::string& passName, const std::vector<DescriptorBinding>& bindings, std::function<void(RgPassBuilder& builder)> setupFunc, std::function<void(RgCommandList& cmd)> executeFunc)
{
	RgPassNode newNode{};
	newNode.Name = passName;
	newNode.DescriptorBindings = bindings;
	newNode.ExecuteCallback = executeFunc;

	RgPassBuilder builder(newNode);
	setupFunc(builder);
	
	m_PassNodes.push_back(builder.GetNode());
}

void RenderGraph::AddOutput(const RgTextureHandle& handle)
{
	auto& view = m_PhysicalViews[handle.Id];
	view.UsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	view.IsOutput = true;
}

std::vector<RgResourceView> RenderGraph::GetOutputs()
{
	std::vector<RgResourceView> result;
	for (const auto& view : m_PhysicalViews)
	{
		if (view.IsOutput)
		{
			result.push_back(view);
		}
	}
	return result;
}

struct TextureStateTracker
{
	VkImageLayout CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VkPipelineStageFlags AccessStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkAccessFlags AccessMask = 0;
};

void RenderGraph::Compile(const std::vector<DescriptorBinding>& frameBindings)
{
	m_FrameDescriptorSetLayout = VK_NULL_HANDLE;
	m_FrameDescriptorSet = VK_NULL_HANDLE;
	if (frameBindings.size() != 0)
	{
		size_t bindingHash = HashDescriptorBindings(frameBindings);
		for (auto& pooled : m_DescriptorSetPool)
		{
			if (pooled.Hash == bindingHash && pooled.IsFree)
			{
				m_FrameDescriptorSetLayout = pooled.DescriptorSetLayout;
				m_FrameDescriptorSet = pooled.DescriptorSet;
				pooled.IsFree = false;
				break;
			}
		}

		if (m_FrameDescriptorSet == VK_NULL_HANDLE)
		{
			VkDescriptorSetLayout layout = Pipeline::CreateDescriptorSetLayout(m_Device, frameBindings);

			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = m_DescriptorPool;
			allocInfo.descriptorSetCount = 1;
			allocInfo.pSetLayouts = &layout;

			VkDescriptorSet descriptorSet;
			VkResult result = vkAllocateDescriptorSets(m_Device->GetVulkanDevice(), &allocInfo, &descriptorSet);
			if (result != VK_SUCCESS)
			{
				HY_ENGINE_FATAL("Failed to allocate Vulkan descriptor set... vkAllocateDescriptorSets returned {}", (uint16_t)result);
			}

			PooledDescriptorSet pooled;
			pooled.DescriptorSetLayout = layout;
			pooled.DescriptorSet = descriptorSet;
			pooled.Hash = bindingHash;
			pooled.IsFree = false;

			m_DescriptorSetPool.push_back(pooled);

			m_FrameDescriptorSetLayout = pooled.DescriptorSetLayout;
			m_FrameDescriptorSet = pooled.DescriptorSet;
		}
	}

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
		passColorAttachments.clear();

		CompiledPass compiledPass{};
		compiledPass.Name = recordedPass.Name;
		compiledPass.ExecuteCallback = recordedPass.ExecuteCallback;

		if (frameBindings.size() != 0)
		{
			size_t bindingHash = HashDescriptorBindings(recordedPass.DescriptorBindings);
			for (auto& pooled : m_DescriptorSetPool)
			{
				if (pooled.Hash == bindingHash && pooled.IsFree)
				{
					compiledPass.DescriptorSetLayout = pooled.DescriptorSetLayout;
					compiledPass.DescriptorSet = pooled.DescriptorSet;
					pooled.IsFree = false;
					break;
				}
			}

			if (compiledPass.DescriptorSet == VK_NULL_HANDLE)
			{
				VkDescriptorSetLayout layout = Pipeline::CreateDescriptorSetLayout(m_Device, recordedPass.DescriptorBindings);

				VkDescriptorSetAllocateInfo allocInfo{};
				allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
				allocInfo.descriptorPool = m_DescriptorPool;
				allocInfo.descriptorSetCount = 1;
				allocInfo.pSetLayouts = &layout;

				VkDescriptorSet descriptorSet;
				VkResult result = vkAllocateDescriptorSets(m_Device->GetVulkanDevice(), &allocInfo, &descriptorSet);
				if (result != VK_SUCCESS)
				{
					HY_ENGINE_FATAL("Failed to allocate Vulkan descriptor set... vkAllocateDescriptorSets returned {}", (uint16_t)result);
				}

				PooledDescriptorSet pooled;
				pooled.DescriptorSetLayout = layout;
				pooled.DescriptorSet = descriptorSet;
				pooled.Hash = bindingHash;
				pooled.IsFree = false;

				m_DescriptorSetPool.push_back(pooled);

				compiledPass.DescriptorSetLayout = pooled.DescriptorSetLayout;
				compiledPass.DescriptorSet = pooled.DescriptorSet;
			}
		}

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
			else if (view.IsOutput && state.CurrentLayout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
			{
				VkBarrierCommand finalCmd{};
				finalCmd.TextureId = i;
				finalCmd.SrcStage = state.AccessStage;
				finalCmd.DstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

				finalCmd.Barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				finalCmd.Barrier.oldLayout = state.CurrentLayout;
				finalCmd.Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
				state.CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
		}
	}
}

void RenderGraph::Execute(VkCommandBuffer cmdBuffer, const std::vector<DescriptorBindingValue>& bindingValues)
{
	for (uint32_t i = 0; i < bindingValues.size(); i++)
	{
		switch (bindingValues[i].Type)
		{
		case DescriptorType::UniformBuffer:
			for (uint32_t j = 0; j < bindingValues[i].RenderBuffers.size(); j++)
			{
				VkDescriptorBufferInfo bufferInfo{};
				bufferInfo.buffer = bindingValues[i].RenderBuffers[j]->GetBuffer();
				bufferInfo.offset = 0;
				bufferInfo.range = bindingValues[i].RenderBuffers[j]->GetSize();

				VkWriteDescriptorSet descriptorWrite{};
				descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				descriptorWrite.dstSet = m_FrameDescriptorSet;
				descriptorWrite.dstBinding = i;
				descriptorWrite.dstArrayElement = j;

				descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				descriptorWrite.descriptorCount = 1;

				descriptorWrite.pBufferInfo = &bufferInfo;
				descriptorWrite.pImageInfo = nullptr;

				vkUpdateDescriptorSets(m_Device->GetVulkanDevice(), 1, &descriptorWrite, 0, nullptr);
			}
			break;

		case DescriptorType::StorageBuffer:
			HY_ASSERT(false, "Not implemented");
			break;

		case DescriptorType::CombinedImageSampler:
			HY_ASSERT(false, "Not implemented");
			break;
		}
	}

	m_CommandList.InitFrame(cmdBuffer, &m_PhysicalViews, m_FrameDescriptorSet);

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

		std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
		if (m_FrameDescriptorSetLayout != VK_NULL_HANDLE)
		{
			descriptorSetLayouts.push_back(m_FrameDescriptorSetLayout);
		}
		if (pass.DescriptorSetLayout)
		{
			descriptorSetLayouts.push_back(pass.DescriptorSetLayout);
		}

		m_CommandList.InitPass(pass.RenderPass, descriptorSetLayouts, pass.DescriptorSet);

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
