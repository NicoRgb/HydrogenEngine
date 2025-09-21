#pragma once

#include "Hydrogen/Renderer/RenderAPI.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanCommandQueue.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

namespace Hydrogen
{
	class VulkanRenderAPI : public RenderAPI
	{
	public:
		VulkanRenderAPI(const std::shared_ptr<RenderContext>& renderContext);
		~VulkanRenderAPI();

		void BeginFrame() override;
		void SubmitFrame(const std::shared_ptr<CommandQueue>& commandQueue) override;

		bool FrameFinished() override { return m_FrameFinished; }
		Event<>& GetFrameFinishedEvent() override { return m_FrameFinishedEvent; }

		struct FrameInfo
		{
			uint32_t swapChainImageIndex;
		};

		const FrameInfo& GetCurrentFrame() const { return m_CurrentFrame; }

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;

		FrameInfo m_CurrentFrame;

		VkSemaphore m_ImageAvailableSemaphore;
		VkSemaphore m_RenderFinishedSemaphore;
		VkFence m_InFlightFence;

		bool m_FrameFinished;
		Event<> m_FrameFinishedEvent;
	};
}
