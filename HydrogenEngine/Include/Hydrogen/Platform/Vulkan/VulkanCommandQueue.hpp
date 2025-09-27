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

		void BindPipeline(const std::shared_ptr<Pipeline>& pipeline, const std::shared_ptr<Framebuffer>& framebuffer) override;
		void UnbindPipeline() override;

		void BindVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) override;
		void BindIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

		void SetViewport() override;
		void SetScissor() override;

		void Draw() override;
		void DrawIndexed(const std::shared_ptr<IndexBuffer>& indexBuffer) override;

		const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return m_CommandBuffers; }

	private:
		const std::shared_ptr<class VulkanRenderContext> m_RenderContext;
		std::shared_ptr<class VulkanRenderAPI> m_RenderAPI;

		std::vector<VkCommandBuffer> m_CommandBuffers;
	};
}
