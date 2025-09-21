#include "Hydrogen/Platform/Vulkan/VulkanRenderAPI.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Core.hpp"

#include <vulkan/vulkan.h>

using namespace Hydrogen;

VulkanRenderAPI::VulkanRenderAPI(const std::shared_ptr<RenderContext>& renderContext)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)), m_CurrentFrame(0), m_FrameFinished(true)
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	HY_ASSERT(vkCreateSemaphore(m_RenderContext->GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore) == VK_SUCCESS, "Failed to create vulkan semaphore");
	HY_ASSERT(vkCreateSemaphore(m_RenderContext->GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphore) == VK_SUCCESS, "Failed to create vulkan semaphore");
	HY_ASSERT(vkCreateFence(m_RenderContext->GetDevice(), &fenceInfo, nullptr, &m_InFlightFence) == VK_SUCCESS, "Failed to create vulkan fence");
}

VulkanRenderAPI::~VulkanRenderAPI()
{
	vkDeviceWaitIdle(m_RenderContext->GetDevice());

	vkDestroySemaphore(m_RenderContext->GetDevice(), m_ImageAvailableSemaphore, nullptr);
	vkDestroySemaphore(m_RenderContext->GetDevice(), m_RenderFinishedSemaphore, nullptr);
	vkDestroyFence(m_RenderContext->GetDevice(), m_InFlightFence, nullptr);
}

void VulkanRenderAPI::BeginFrame()
{
	m_FrameFinished = false;

	vkWaitForFences(m_RenderContext->GetDevice(), 1, &m_InFlightFence, VK_TRUE, UINT64_MAX);

	VkResult result = vkAcquireNextImageKHR(m_RenderContext->GetDevice(), m_RenderContext->GetSwapChain(), UINT64_MAX, m_ImageAvailableSemaphore, VK_NULL_HANDLE, &m_CurrentFrame.swapChainImageIndex);

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

	vkResetFences(m_RenderContext->GetDevice(), 1, &m_InFlightFence);
}

void VulkanRenderAPI::SubmitFrame(const std::shared_ptr<CommandQueue>& commandQueue)
{
	const auto vkCommandBuffer = CommandQueue::Get<VulkanCommandQueue>(commandQueue)->GetCommandBuffer();

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vkCommandBuffer;

	VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	HY_ASSERT(vkQueueSubmit(m_RenderContext->GetGraphicsQueue(), 1, &submitInfo, m_InFlightFence) == VK_SUCCESS, "Failed to submit vulkan command buffer");

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { m_RenderContext->GetSwapChain() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &m_CurrentFrame.swapChainImageIndex;

	vkQueuePresentKHR(m_RenderContext->GetPresentQueue(), &presentInfo);

	m_FrameFinished = true;
	m_FrameFinishedEvent.Invoke();
}
