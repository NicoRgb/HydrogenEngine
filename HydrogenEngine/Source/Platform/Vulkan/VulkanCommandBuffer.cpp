#include "Hydrogen/Platform/Vulkan/VulkanCommandBuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderTarget.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanIndexBuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanPipeline.hpp"
#include "Hydrogen/Core.hpp"
#include <vulkan/vulkan.h>

using namespace Hydrogen;

VulkanCommandBuffer::VulkanCommandBuffer(const std::shared_ptr<RenderContext>& renderContext)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext))
	, m_CurrentFrameContext{}
	, m_IsFrameFinished(true)
	, m_IsRecording(false)
{
	m_Frames.resize(m_RenderContext->GetMaxFramesInFlight());
	m_InFlightImages.resize(m_RenderContext->GetSwapChainImages().size(), VK_NULL_HANDLE);

	CreateSynchronizationObjects();
	CreateCommandBuffers();
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
	vkDeviceWaitIdle(m_RenderContext->GetDevice());
	DestroySynchronizationObjects();
	DestroyCommandBuffers();
}

void VulkanCommandBuffer::CreateSynchronizationObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (auto& frame : m_Frames)
	{
		HY_ASSERT(vkCreateSemaphore(m_RenderContext->GetDevice(), &semaphoreInfo, nullptr, &frame.imageAvailableSemaphore) == VK_SUCCESS,
			"Failed to create image available semaphore");
		HY_ASSERT(vkCreateSemaphore(m_RenderContext->GetDevice(), &semaphoreInfo, nullptr, &frame.renderFinishedSemaphore) == VK_SUCCESS,
			"Failed to create render finished semaphore");
		HY_ASSERT(vkCreateFence(m_RenderContext->GetDevice(), &fenceInfo, nullptr, &frame.inFlightFence) == VK_SUCCESS,
			"Failed to create in-flight fence");
	}
}

void VulkanCommandBuffer::DestroySynchronizationObjects()
{
	for (const auto& frame : m_Frames)
	{
		vkDestroySemaphore(m_RenderContext->GetDevice(), frame.imageAvailableSemaphore, nullptr);
		vkDestroySemaphore(m_RenderContext->GetDevice(), frame.renderFinishedSemaphore, nullptr);
		vkDestroyFence(m_RenderContext->GetDevice(), frame.inFlightFence, nullptr);
	}
}

void VulkanCommandBuffer::CreateCommandBuffers()
{
	const uint32_t framesInFlight = m_RenderContext->GetMaxFramesInFlight();

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_RenderContext->GetCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = framesInFlight;

	std::vector<VkCommandBuffer> commandBuffers(framesInFlight);
	HY_ASSERT(vkAllocateCommandBuffers(m_RenderContext->GetDevice(), &allocInfo, commandBuffers.data()) == VK_SUCCESS,
		"Failed to allocate command buffers");

	for (uint32_t i = 0; i < framesInFlight; ++i)
	{
		m_Frames[i].commandBuffer = commandBuffers[i];
	}
}

void VulkanCommandBuffer::DestroyCommandBuffers()
{
	std::vector<VkCommandBuffer> commandBuffers;
	for (const auto& frame : m_Frames)
	{
		commandBuffers.push_back(frame.commandBuffer);
	}

	if (!commandBuffers.empty())
	{
		vkFreeCommandBuffers(m_RenderContext->GetDevice(), m_RenderContext->GetCommandPool(),
			static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	}
}

const VulkanCommandBuffer::FrameData& VulkanCommandBuffer::GetCurrentFrameData() const
{
	return m_Frames[m_RenderContext->GetCurrentFrame()];
}

void VulkanCommandBuffer::BeginFrame(const std::shared_ptr<RenderTarget>& renderTarget)
{
	m_IsFrameFinished = false;
	m_CurrentFrameContext.renderTarget = renderTarget;

	const auto& frameData = GetCurrentFrameData();

	vkWaitForFences(m_RenderContext->GetDevice(), 1, &frameData.inFlightFence, VK_TRUE, UINT64_MAX);

	if (renderTarget->GetSpec().Type == RenderTargetType::SwapChain)
	{
		VkResult result = vkAcquireNextImageKHR(
			m_RenderContext->GetDevice(),
			m_RenderContext->GetSwapChain(),
			UINT64_MAX,
			frameData.imageAvailableSemaphore,
			VK_NULL_HANDLE,
			&m_CurrentFrameContext.swapChainImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			HY_ENGINE_ERROR("vkAcquireNextImageKHR returned VK_ERROR_OUT_OF_DATE_KHR");
		}
		else if (result == VK_SUBOPTIMAL_KHR)
		{
			HY_ENGINE_WARN("vkAcquireNextImageKHR returned VK_SUBOPTIMAL_KHR");
		}
		else if (result != VK_SUCCESS)
		{
			HY_ASSERT(false, "Failed to acquire next swap chain image");
		}

		if (m_InFlightImages[m_CurrentFrameContext.swapChainImageIndex] != VK_NULL_HANDLE)
		{
			vkWaitForFences(m_RenderContext->GetDevice(), 1,
				&m_InFlightImages[m_CurrentFrameContext.swapChainImageIndex],
				VK_TRUE,
				UINT64_MAX);
		}

		m_InFlightImages[m_CurrentFrameContext.swapChainImageIndex] = frameData.inFlightFence;
	}

	vkResetFences(m_RenderContext->GetDevice(), 1, &frameData.inFlightFence);
}

void VulkanCommandBuffer::EndFrame()
{
	SubmitFrame();
	m_IsFrameFinished = true;
	m_FrameFinishedEvent.Invoke();
	m_RenderContext->SetCurrentFrame((m_RenderContext->GetCurrentFrame() + 1) % m_RenderContext->GetMaxFramesInFlight());
}

void VulkanCommandBuffer::SubmitFrame()
{
	const auto& frameData = GetCurrentFrameData();
	const bool isSwapChainTarget = m_CurrentFrameContext.renderTarget->GetSpec().Type == RenderTargetType::SwapChain;

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { frameData.imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = isSwapChainTarget ? 1 : 0;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &frameData.commandBuffer;

	VkSemaphore signalSemaphores[] = { frameData.renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = isSwapChainTarget ? 1 : 0;
	submitInfo.pSignalSemaphores = signalSemaphores;

	HY_ASSERT(vkQueueSubmit(m_RenderContext->GetGraphicsQueue(), 1, &submitInfo, frameData.inFlightFence) == VK_SUCCESS,
		"Failed to submit command buffer");

	if (isSwapChainTarget)
	{
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { m_RenderContext->GetSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &m_CurrentFrameContext.swapChainImageIndex;

		vkQueuePresentKHR(m_RenderContext->GetPresentQueue(), &presentInfo);
	}
}

void VulkanCommandBuffer::StartRecording(const std::shared_ptr<RenderTarget>& renderTarget)
{
	HY_ASSERT(!m_IsRecording, "Already recording");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	HY_ASSERT(vkBeginCommandBuffer(GetCurrentVkCommandBuffer(), &beginInfo) == VK_SUCCESS,
		"Failed to begin command buffer");

	m_IsRecording = true;

	BeginRenderPass(renderTarget);
}

void VulkanCommandBuffer::EndRecording()
{
	EndRenderPass();

	HY_ASSERT(m_IsRecording, "Not recording");
	HY_ASSERT(vkEndCommandBuffer(GetCurrentVkCommandBuffer()) == VK_SUCCESS,
		"Failed to end command buffer");

	m_IsRecording = false;
}

void VulkanCommandBuffer::BindPipeline(const std::shared_ptr<Pipeline>& pipeline)
{
	const auto vulkanPipeline = Pipeline::Get<VulkanPipeline>(pipeline);
	vkCmdBindPipeline(GetCurrentVkCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->GetPipeline());

	vkCmdBindDescriptorSets(GetCurrentVkCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->GetPipelineLayout(),
		0, 1, &vulkanPipeline->GetDescriptorSets()[m_RenderContext->GetCurrentFrame()], 0, nullptr);
}

void VulkanCommandBuffer::BindVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer)
{
	VkBuffer vertexBuffers[] = { VertexBuffer::Get<VulkanVertexBuffer>(vertexBuffer)->GetBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(GetCurrentVkCommandBuffer(), 0, 1, vertexBuffers, offsets);
}

void VulkanCommandBuffer::BindDynamicVertexBuffer(const std::shared_ptr<DynamicVertexBuffer>& vertexBuffer)
{
	VkBuffer vertexBuffers[] = { DynamicVertexBuffer::Get<VulkanDynamicVertexBuffer>(vertexBuffer)->GetBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(GetCurrentVkCommandBuffer(), 0, 1, vertexBuffers, offsets);
}

void VulkanCommandBuffer::BindIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer)
{
	auto buffer = IndexBuffer::Get<VulkanIndexBuffer>(indexBuffer)->GetBuffer();
	vkCmdBindIndexBuffer(GetCurrentVkCommandBuffer(), buffer, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanCommandBuffer::SetViewport(const std::shared_ptr<RenderTarget>& renderTarget)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)renderTarget->GetWidth();
	viewport.height = (float)renderTarget->GetHeight();
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	vkCmdSetViewport(GetCurrentVkCommandBuffer(), 0, 1, &viewport);
}

void VulkanCommandBuffer::SetScissor(const std::shared_ptr<RenderTarget>& renderTarget)
{
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { renderTarget->GetWidth(), renderTarget->GetHeight() };

	vkCmdSetScissor(GetCurrentVkCommandBuffer(), 0, 1, &scissor);
}

void VulkanCommandBuffer::UploadPushConstants(const std::shared_ptr<Pipeline>& pipeline, uint32_t index, void* data)
{
	const auto vulkanPipeline = Pipeline::Get<VulkanPipeline>(pipeline);
	const auto range = vulkanPipeline->GetPushConstantRanges()[index];
	vkCmdPushConstants(GetCurrentVkCommandBuffer(), vulkanPipeline->GetPipelineLayout(), range.stageFlags, range.offset, range.size, data);
}

void VulkanCommandBuffer::Draw(size_t numVertices)
{
	vkCmdDraw(GetCurrentVkCommandBuffer(), numVertices, 1, 0, 0);
}

void VulkanCommandBuffer::DrawIndexed(const std::shared_ptr<IndexBuffer>& indexBuffer)
{
	vkCmdDrawIndexed(GetCurrentVkCommandBuffer(), (uint32_t)indexBuffer->GetNumIndices(), 1, 0, 0, 0);
}

void VulkanCommandBuffer::BeginRenderPass(const std::shared_ptr<RenderTarget>& renderTarget)
{
	auto vkTarget = RenderTarget::Get<VulkanRenderTarget>(renderTarget);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vkTarget->GetRenderPass();

	uint32_t framebufferIndex = m_RenderContext->GetCurrentFrame();
	if (vkTarget->GetSpec().Type == RenderTargetType::SwapChain)
	{
		framebufferIndex = m_CurrentFrameContext.swapChainImageIndex;
	}

	renderPassInfo.framebuffer = vkTarget->GetFramebuffer(framebufferIndex);
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = { vkTarget->GetWidth(), vkTarget->GetHeight() };

	std::array<VkClearValue, 3> clearValues{};
	clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
	clearValues[1].depthStencil = { 1.0f, 0 };
	clearValues[2].color = { {0.0f, 0.0f, 0.0f, 1.0f} };

	uint32_t clearCount = 0;
	if (vkTarget->GetSpec().Type == RenderTargetType::SwapChain || vkTarget->GetColorTexture())
		clearCount++;
	if (vkTarget->GetDepthImage() != VK_NULL_HANDLE)
		clearCount++;
	if (vkTarget->GetResolveImage() != VK_NULL_HANDLE)
		clearCount++;

	renderPassInfo.clearValueCount = clearCount;
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(GetCurrentVkCommandBuffer(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanCommandBuffer::EndRenderPass()
{
	vkCmdEndRenderPass(GetCurrentVkCommandBuffer());
}
