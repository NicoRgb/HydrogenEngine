#pragma once

#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Renderer/RenderBuffer.hpp"
#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Core.hpp"

#include <vma/vk_mem_alloc.h>

namespace Hydrogen
{
	struct RgTextureHandle { size_t Id = uint64_t(-1); bool IsValid() const { return Id != uint64_t(-1); } };

	struct RgTextureDesc
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		TextureFormat Format = TextureFormat::RGBA8_SRGB;
	};

	struct RgResourceView
	{
		VkImage Image = VK_NULL_HANDLE;
		VkImageView ImageView = VK_NULL_HANDLE;
		VkImageUsageFlags UsageFlags = 0;
		bool IsImported = false;
	};

	class RgCommandList
	{
	public:
		RgCommandList(RenderDevice* device)
			: m_Device(device) {}

		void InitFrame(VkCommandBuffer cmdBuf, const std::vector<RgResourceView>* physicalViews)
		{
			m_CmdBuf = cmdBuf;
			m_PhysicalViews = physicalViews;
		}

		void InitPass(VkRenderPass renderPass)
		{
			m_RenderPass = renderPass;
		}

		VkCommandBuffer GetCommandBuffer() const { return m_CmdBuf; }

		RgResourceView GetTextureView(RgTextureHandle handle) const
		{
			return (*m_PhysicalViews)[handle.Id];
		}

		void BindPipeline(const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader, PipelineSpec spec);
		void Draw(uint32_t vertexCount);

	private:
		RenderDevice* m_Device;

		VkCommandBuffer m_CmdBuf = VK_NULL_HANDLE;
		const std::vector<RgResourceView>* m_PhysicalViews = nullptr;

		VkRenderPass m_RenderPass = VK_NULL_HANDLE;

		std::unordered_map<size_t, std::unique_ptr<Pipeline>> m_PipelineCache;
	};

	struct RgResourceUsage
	{
		enum class Type { ColorWrite, DepthWrite, ShaderRead };

		Type UsageType;
		RgTextureHandle Handle;
	};

	struct RgPassNode
	{
		std::string Name;
		std::vector<RgResourceUsage> Usages;
		std::function<void(RgCommandList&)> ExecuteCallback;
	};

	class RgPassBuilder
	{
	public:
		RgPassBuilder(const RgPassNode& node)
			: m_PassNode(node) {}
		~RgPassBuilder() = default;

		RgTextureHandle WriteColor(RgTextureHandle texture);
		RgTextureHandle WriteDepth(RgTextureHandle texture);
		RgTextureHandle ReadTexture(RgTextureHandle texture);

		RgPassNode GetNode() const { return m_PassNode; }

	private:
		RgPassNode m_PassNode;
	};

	struct RgTextureNode
	{
		RgTextureDesc Desc;
		bool IsImported = false;
	};

	struct VkBarrierCommand
	{
		size_t TextureId;
		VkImageMemoryBarrier Barrier;
		VkPipelineStageFlags SrcStage;
		VkPipelineStageFlags DstStage;
	};

	struct CompiledPass
	{
		std::string Name;
		std::vector<VkBarrierCommand> PrePassBarriers;
		std::function<void(RgCommandList&)> ExecuteCallback;

		std::vector<VkFormat> ColorFormats;
		std::optional<VkFormat> DepthFormat;

		std::vector<VkImageView> ColorAttachments;
		VkImageView DepthAttachment = VK_NULL_HANDLE;
		VkExtent2D RenderExtent;

		VkRenderPass RenderPass;
		VkFramebuffer Framebuffer;

		std::vector<VkClearValue> ClearValues;
	};

	class RenderGraph
	{
	public:
		RenderGraph(RenderDevice* device);
		~RenderGraph();

		void Reset();
		void ClearCache();

		RgTextureHandle CreateTexture(const RgTextureDesc& desc);
		RgTextureHandle ImportTexture(VkImage image, VkImageView imageView, const RgTextureDesc& desc);

		void AddPass(const std::string& name,
			std::function<void(RgPassBuilder&)> setup,
			std::function<void(RgCommandList&)> execute);

		void Compile();
		void Execute(VkCommandBuffer cmdBuffer);

		VkRenderPass GetRenderPass(std::string passName)
		{
			for (const auto& pass : m_CompiledPasses)
			{
				if (pass.Name == passName)
					return pass.RenderPass;
			}
			return VK_NULL_HANDLE;
		}

	private:
		struct RenderPassAttachment
		{
			VkFormat Format;
			VkImageLayout InitialLayout;
			VkAttachmentLoadOp LoadOp;
		};
		VkRenderPass GetOrCreateRenderPass(const std::vector<RenderPassAttachment>& colors, std::optional<RenderPassAttachment> depth);
		VkFramebuffer GetOrCreateFramebuffer(VkRenderPass rp, const std::vector<VkImageView>& views, VkImageView depthView, VkExtent2D extent);

		VkImage CreatePhysicalImage(const RgTextureDesc& desc, VkImageUsageFlags usage, VmaAllocation* outAllocation);
		VkImageView CreatePhysicalImageView(VkImage image, const RgTextureDesc& desc, VkImageUsageFlags usage);

		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

		RenderDevice* m_Device;
		RgCommandList m_CommandList;

		std::vector<RgTextureDesc> m_TextureDescs;
		std::vector<RgResourceView> m_PhysicalViews;
		std::vector<RgPassNode> m_PassNodes;
		std::vector<CompiledPass> m_CompiledPasses;
		std::vector<VkBarrierCommand> m_PostRenderBarriers;

		std::unordered_map<size_t, VkRenderPass> m_RenderPassCache;
		std::unordered_map<size_t, VkFramebuffer> m_FramebufferCache;

		struct PooledTexture
		{
			VkImage Image = VK_NULL_HANDLE;
			VkImageView View = VK_NULL_HANDLE;
			VmaAllocation Allocation = VK_NULL_HANDLE;
			size_t Hash = 0;
			bool IsFree = true;
		};

		std::vector<PooledTexture> m_PhysicalTexturePool;
	};
}
