#pragma once

#include "Hydrogen/Renderer/CommandBuffer.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanRenderContext;

	class VulkanCommandBuffer : public CommandBuffer
	{
	public:
		VulkanCommandBuffer(const std::shared_ptr<RenderContext>& renderContext);
		~VulkanCommandBuffer();

		void BeginFrame(const std::shared_ptr<RenderTarget>& renderTarget) override;
		void EndFrame() override;

		void StartRecording(const std::shared_ptr<RenderTarget>& renderTarget) override;
		void EndRecording() override;

		void BindPipeline(const std::shared_ptr<Pipeline>& pipeline) override;
		void BindVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) override;
		void BindDynamicVertexBuffer(const std::shared_ptr<DynamicVertexBuffer>& vertexBuffer) override;
		void BindIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

		void SetViewport(const std::shared_ptr<RenderTarget>& renderTarget) override;
		void SetScissor(const std::shared_ptr<RenderTarget>& renderTarget) override;

		void UploadPushConstants(const std::shared_ptr<Pipeline>& pipeline, uint32_t index, void* data) override;

		void Draw(size_t numVertices) override;
		void DrawIndexed(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

		bool IsFrameFinished() const override { return m_IsFrameFinished; }
		Event<>& GetFrameFinishedEvent() override { return m_FrameFinishedEvent; }

		VkCommandBuffer GetCurrentVkCommandBuffer() const { return GetCurrentFrameData().commandBuffer; }

	private:
		struct FrameData
		{
			VkCommandBuffer commandBuffer;
			VkSemaphore imageAvailableSemaphore;
			VkSemaphore renderFinishedSemaphore;
			VkFence inFlightFence;
		};

		struct FrameContext
		{
			std::shared_ptr<RenderTarget> renderTarget;
			uint32_t swapChainImageIndex;
		};

		void SubmitFrame();
		void CreateSynchronizationObjects();
		void DestroySynchronizationObjects();
		void CreateCommandBuffers();
		void DestroyCommandBuffers();

		void BeginRenderPass(const std::shared_ptr<RenderTarget>& renderTarget);
		void EndRenderPass();

		const FrameData& GetCurrentFrameData() const;

		std::shared_ptr<VulkanRenderContext> m_RenderContext;
		std::vector<FrameData> m_Frames;
		std::vector<VkFence> m_InFlightImages;

		FrameContext m_CurrentFrameContext;
		bool m_IsFrameFinished;
		bool m_IsRecording;
		Event<> m_FrameFinishedEvent;
	};
}
