#pragma once

#include "Hydrogen/Renderer/CommandQueue.hpp"

#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanCommandQueue : public CommandQueue
	{
	public:
		VulkanCommandQueue(const std::shared_ptr<RenderContext>& renderContext);
		~VulkanCommandQueue();

		void StartRecording(const std::shared_ptr<RenderAPI>& renderAPI) override;
		void EndRecording() override;

		void BeginRenderPass(const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<Framebuffer>& framebuffer) override;
		void EndRenderPass() override;

		void BindPipeline(const std::shared_ptr<Pipeline>& pipeline) override;
		void BindVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) override;
		void BindIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

		void SetViewport(const std::shared_ptr<Framebuffer>& framebuffer) override;
		void SetScissor(const std::shared_ptr<Framebuffer>& framebuffer) override;

		void UploadPushConstants(const std::shared_ptr<Pipeline>& pipeline, uint32_t index, void* data) override;

		void Draw() override;
		void DrawIndexed(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

		const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return m_CommandBuffers; }

	private:
		const std::shared_ptr<class VulkanRenderContext> m_RenderContext;
		std::shared_ptr<class VulkanRenderAPI> m_RenderAPI;

		std::vector<VkCommandBuffer> m_CommandBuffers;
	};
}
