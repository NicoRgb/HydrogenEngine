#pragma once

#include "Hydrogen/Renderer/RenderAPI.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanCommandQueue.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"

namespace Hydrogen
{
	class VulkanRenderAPI : public RenderAPI
	{
	public:
		VulkanRenderAPI(const std::shared_ptr<RenderContext>& renderContext);
		~VulkanRenderAPI();

		void BeginFrame(const std::shared_ptr<Framebuffer>& framebuffer) override;
		void SubmitFrame(const std::shared_ptr<CommandQueue>& commandQueue) override;

		bool FrameFinished() override { return m_FrameFinished; }
		Event<>& GetFrameFinishedEvent() override { return m_FrameFinishedEvent; }

		struct FrameInfo
		{
			std::shared_ptr<Framebuffer> framebuffer;
			uint32_t swapChainImageIndex;
		};

		const FrameInfo& GetCurrentFrame() const { return m_CurrentFrame; }

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;

		FrameInfo m_CurrentFrame;

		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> m_RenderFinishedSemaphores;
		std::vector<VkFence> m_InFlightFences;

		bool m_FrameFinished;
		Event<> m_FrameFinishedEvent;
	};
}
