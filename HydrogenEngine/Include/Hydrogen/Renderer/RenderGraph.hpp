#pragma once

#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Renderer/RenderBuffer.hpp"
#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Core.hpp"

#include <vma/vk_mem_alloc.h>

namespace Hydrogen
{
	struct RgResourceHandle { size_t Id = uint64_t(-1); bool IsValid() const { return Id != uint64_t(-1); } };

	struct RgTextureDesc
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		TextureFormat Format = TextureFormat::RGBA8_SRGB;
	};

	enum class RgBufferType
	{
		Uniform,
		Storage
	};
	
	struct RgBufferDesc
	{
		uint32_t Size = 0;
		RgBufferType Type = RgBufferType::Uniform;
	};

	struct RgTextureView
	{
		VkImage Image = VK_NULL_HANDLE;
		VkImageView ImageView = VK_NULL_HANDLE;
		VkImageUsageFlags UsageFlags = 0;
		bool IsImported = false;
		bool IsOutput = false;
	};

	class RgCommandList
	{
	public:
		RgCommandList(RenderDevice* device)
			: m_Device(device) {}

		void InitFrame(VkCommandBuffer cmdBuf, const std::vector<RgTextureView>* physicalViews, VkDescriptorSet frameDescriptorSet)
		{
			m_CmdBuf = cmdBuf;
			m_PhysicalViews = physicalViews;
			m_FrameDescriptorSet = frameDescriptorSet;
		}

		void InitPass(VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, VkDescriptorSet passDescriptorSet)
		{
			m_RenderPass = renderPass;
			m_DescriptorSetLayouts = descriptorSetLayouts;
			m_PassDescriptorSet = passDescriptorSet;
		}

		VkCommandBuffer GetCommandBuffer() const { return m_CmdBuf; }

		RgTextureView GetTextureView(RgResourceHandle handle) const
		{
			return (*m_PhysicalViews)[handle.Id];
		}

		void PushConstants(const void* data, uint32_t size, uint32_t offset, ShaderStage stageFlags);
		void BindPipeline(const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader, PipelineSpec spec);
		void BindVertexBuffer(const RenderBuffer* vertexBuffer);
		void BindIndexBuffer(const RenderBuffer* indexBuffer);
		void Draw(uint32_t vertexCount);
		void DrawIndexed(uint32_t indexCount);

	private:
		RenderDevice* m_Device;

		VkCommandBuffer m_CmdBuf = VK_NULL_HANDLE;
		const std::vector<RgTextureView>* m_PhysicalViews = nullptr;
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
		VkDescriptorSet m_FrameDescriptorSet = VK_NULL_HANDLE;
		VkDescriptorSet m_PassDescriptorSet = VK_NULL_HANDLE;

		Pipeline* m_BoundPipeline = nullptr;

		std::unordered_map<size_t, std::unique_ptr<Pipeline>> m_PipelineCache;
	};

	struct RgResourceUsage
	{
		enum class Type { ColorWrite, DepthWrite, ShaderRead };

		Type UsageType;
		RgResourceHandle Handle;
	};

	struct RgPassNode
	{
		std::string Name;
		std::vector<RgResourceUsage> Usages;
		std::vector<DescriptorBinding> DescriptorBindings;
		std::vector<DescriptorBindingValue> DescriptorBindingValues;
		std::function<void(RgCommandList&)> ExecuteCallback;
	};

	class RgPassBuilder
	{
	public:
		RgPassBuilder(const RgPassNode& node)
			: m_PassNode(node) {}
		~RgPassBuilder() = default;

		RgResourceHandle WriteColor(RgResourceHandle texture);
		RgResourceHandle WriteDepth(RgResourceHandle texture);
		RgResourceHandle ReadTexture(RgResourceHandle texture);

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

		VkDescriptorSetLayout DescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;

		std::vector<VkClearValue> ClearValues;
	};

	#define FREE_AFTER_UNUSED_FRAMES 500
	#define CLEAR_INACTIVE_THREASHHOLD 50

	class RenderGraph
	{
	public:
		RenderGraph(RenderDevice* device, uint32_t maxFIF);
		~RenderGraph();

		void Reset();

		void ClearCache();

		RgResourceHandle CreateTexture(const RgTextureDesc& desc);
		RgResourceHandle ImportTexture(VkImage image, VkImageView imageView, const RgTextureDesc& desc);

		void AddPass(const std::string& name,
			const std::vector<DescriptorBinding>& bindings, // set 1
			const std::vector<DescriptorBindingValue>& bindingValues,
			std::function<void(RgPassBuilder&)> setup,
			std::function<void(RgCommandList&)> execute);

		void AddOutput(const RgResourceHandle& handle);
		std::vector<RgTextureView> GetOutputs();

		void Compile(const std::vector<DescriptorBinding>& frameBindings); // set 0
		void Execute(VkCommandBuffer cmdBuffer, const std::vector<DescriptorBindingValue>& bindingValues);

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
		uint32_t m_MaxFIF;
		uint32_t m_FrameIndex;

		void ResetRecording();
		void ResetCompilation();

		void CreateDescriptorPool();
		void CreateSampler();

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

		uint32_t GetOrCreateBuffer(const RgBufferDesc& desc);
		void UploadDataToBuffer(void* data, size_t size, void* mapped);

		void UpdateDescriptorSet(const std::vector<DescriptorBinding>& descriptorBindings, const std::vector<DescriptorBindingValue>& bindingValues, VkDescriptorSet descriptorSet);
		VkDescriptorBufferInfo PrepareAndUploadBuffer(const DescriptorBindingValue& val, RgBufferType type);

		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

		RenderDevice* m_Device;
		RgCommandList m_CommandList;

		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		VkSampler m_Sampler = VK_NULL_HANDLE;

		VkDescriptorSetLayout m_FrameDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet m_FrameDescriptorSet = VK_NULL_HANDLE;

		std::vector<RgTextureDesc> m_TextureDescs;
		std::vector<RgTextureView> m_PhysicalTextureViews;
		std::vector<RgPassNode> m_PassNodes;

		std::vector<CompiledPass> m_CompiledPasses;
		std::vector<VkBarrierCommand> m_PostRenderBarriers;

		std::unordered_map<size_t, VkRenderPass> m_RenderPassCache;
		std::unordered_map<size_t, VkFramebuffer> m_FramebufferCache;

		struct PooledDescriptorSet
		{
			VkDescriptorSet DescriptorSet = VK_NULL_HANDLE;
			VkDescriptorSetLayout DescriptorSetLayout = VK_NULL_HANDLE;
			size_t Hash = 0;
			bool IsFree = true;

			uint32_t FrameIndex = 0;
		};

		std::vector<PooledDescriptorSet> m_DescriptorSetPool;

		struct PooledTexture
		{
			VkImage Image = VK_NULL_HANDLE;
			VkImageView View = VK_NULL_HANDLE;
			VmaAllocation Allocation = VK_NULL_HANDLE;
			size_t Hash = 0;
			bool IsFree = true;
			
			size_t FramesUnsued = 0;
			bool Active = true;
		};

		std::vector<PooledTexture> m_PhysicalTexturePool;

		struct PooledBuffer
		{
			VkBuffer Buffer = VK_NULL_HANDLE;
			VmaAllocation Allocation = VK_NULL_HANDLE;
			size_t Hash = 0;
			uint32_t Size = 0;
			void* MappedMemory = nullptr;
			bool IsMapped = false;
			bool IsFree = true;

			uint32_t FrameIndex = 0;

			size_t FramesUnsued = 0;
			bool Active = true;
		};

		std::vector<PooledBuffer> m_PhysicalBufferPool;
		std::vector<DescriptorBinding> m_FrameDescriptorBindings;
	};
}
