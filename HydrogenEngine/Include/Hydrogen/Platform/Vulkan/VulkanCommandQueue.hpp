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

		void SetViewport() override;
		void SetScissor() override;

		void Draw() override;

		const VkCommandBuffer GetCommandBuffer() const { return m_CommandBuffer; }

	private:
		const std::shared_ptr<class VulkanRenderContext> m_RenderContext;
		std::shared_ptr<class VulkanRenderAPI> m_RenderAPI;

		VkCommandBuffer m_CommandBuffer;
	};
}
