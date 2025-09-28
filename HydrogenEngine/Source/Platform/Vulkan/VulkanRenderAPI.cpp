#include "Hydrogen/Platform/Vulkan/VulkanRenderAPI.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Core.hpp"

#include <vulkan/vulkan.h>

using namespace Hydrogen;

VulkanRenderAPI::VulkanRenderAPI(const std::shared_ptr<RenderContext>& renderContext)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)), m_CurrentFrame(0), m_FrameFinished(true)
{
	m_ImageAvailableSemaphores.resize(m_RenderContext->GetMaxFramesInFlight());
	m_RenderFinishedSemaphores.resize(m_RenderContext->GetMaxFramesInFlight());
	m_InFlightFences.resize(m_RenderContext->GetMaxFramesInFlight());

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < m_ImageAvailableSemaphores.size(); i++)
	{
		HY_ASSERT(vkCreateSemaphore(m_RenderContext->GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) == VK_SUCCESS, "Failed to create vulkan semaphore");
		HY_ASSERT(vkCreateSemaphore(m_RenderContext->GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) == VK_SUCCESS, "Failed to create vulkan semaphore");
		HY_ASSERT(vkCreateFence(m_RenderContext->GetDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) == VK_SUCCESS, "Failed to create vulkan fence");
	}
}

VulkanRenderAPI::~VulkanRenderAPI()
{
	vkDeviceWaitIdle(m_RenderContext->GetDevice());

	for (size_t i = 0; i < m_ImageAvailableSemaphores.size(); i++)
	{
		vkDestroySemaphore(m_RenderContext->GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_RenderContext->GetDevice(), m_RenderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_RenderContext->GetDevice(), m_InFlightFences[i], nullptr);
	}
}

void VulkanRenderAPI::BeginFrame(const std::shared_ptr<Framebuffer>& framebuffer)
{
	m_FrameFinished = false;

	vkWaitForFences(m_RenderContext->GetDevice(), 1, &m_InFlightFences[m_RenderContext->GetCurrentFrame()], VK_TRUE, UINT64_MAX);

	m_CurrentFrame.framebuffer = framebuffer;
	if (!framebuffer->RenderToTexture())
	{
		VkResult result = vkAcquireNextImageKHR(m_RenderContext->GetDevice(), m_RenderContext->GetSwapChain(), UINT64_MAX, m_ImageAvailableSemaphores[m_RenderContext->GetCurrentFrame()], VK_NULL_HANDLE, &m_CurrentFrame.swapChainImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			HY_ENGINE_ERROR("vkAcquireNextImageKHR returned VK_ERROR_OUT_OF_DATE_KHR");
		}
		if (result == VK_SUBOPTIMAL_KHR)
		{
			HY_ENGINE_WARN("vkAcquireNextImageKHR returned VK_SUBOPTIMAL_KHR");
		}
		else if (result != VK_SUCCESS)
		{
			HY_ASSERT(false, "Failed to acquire next swap chain image");
		}
	}

	vkResetFences(m_RenderContext->GetDevice(), 1, &m_InFlightFences[m_RenderContext->GetCurrentFrame()]);
}

void VulkanRenderAPI::SubmitFrame(const std::shared_ptr<CommandQueue>& commandQueue)
{
	const auto vkCommandBuffer = CommandQueue::Get<VulkanCommandQueue>(commandQueue)->GetCommandBuffers()[m_RenderContext->GetCurrentFrame()];

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_RenderContext->GetCurrentFrame()] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = m_CurrentFrame.framebuffer->RenderToTexture() ? 0 : 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkCommandBuffer;

	VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_RenderContext->GetCurrentFrame()] };
	submitInfo.signalSemaphoreCount = m_CurrentFrame.framebuffer->RenderToTexture() ? 0 : 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	HY_ASSERT(vkQueueSubmit(m_RenderContext->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_RenderContext->GetCurrentFrame()]) == VK_SUCCESS, "Failed to submit vulkan command buffer");

	if (!m_CurrentFrame.framebuffer->RenderToTexture())
	{
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { m_RenderContext->GetSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &m_CurrentFrame.swapChainImageIndex;

		vkQueuePresentKHR(m_RenderContext->GetPresentQueue(), &presentInfo);
	}

	m_FrameFinished = true;
	m_FrameFinishedEvent.Invoke();

	m_RenderContext->SetCurrentFrame((m_RenderContext->GetCurrentFrame() + 1) % m_RenderContext->GetMaxFramesInFlight());
}
