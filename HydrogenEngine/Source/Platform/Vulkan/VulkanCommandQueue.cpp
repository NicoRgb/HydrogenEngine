#include "Hydrogen/Platform/Vulkan/VulkanCommandQueue.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderAPI.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanVertexBuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanIndexBuffer.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

VulkanCommandQueue::VulkanCommandQueue(const std::shared_ptr<RenderContext>& renderContext)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext))
{
	m_CommandBuffers.resize(m_RenderContext->GetMaxFramesInFlight());
		
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_RenderContext->GetCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)m_RenderContext->GetMaxFramesInFlight();

	HY_ASSERT(vkAllocateCommandBuffers(m_RenderContext->GetDevice(), &allocInfo, m_CommandBuffers.data()) == VK_SUCCESS, "Failed to allocate vulkan command buffer");
}

VulkanCommandQueue::~VulkanCommandQueue()
{
}

void VulkanCommandQueue::StartRecording(const std::shared_ptr<RenderAPI>& renderAPI)
{
	m_RenderAPI = RenderAPI::Get<VulkanRenderAPI>(renderAPI);

	vkResetCommandBuffer(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	HY_ASSERT(vkBeginCommandBuffer(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], &beginInfo) == VK_SUCCESS, "Failed to begin recording vulkan command buffer");
}

void VulkanCommandQueue::EndRecording()
{
	m_RenderAPI = nullptr;
	HY_ASSERT(vkEndCommandBuffer(m_CommandBuffers[m_RenderContext->GetCurrentFrame()]) == VK_SUCCESS, "Failed to record vulkan command buffer");
}

void VulkanCommandQueue::BindPipeline(const std::shared_ptr<Pipeline>& pipeline, const std::shared_ptr<Framebuffer>& framebuffer)
{
	const auto vulkanPipeline = Pipeline::Get<VulkanPipeline>(pipeline);
	const auto vulkanFramebuffer = Framebuffer::Get<VulkanFramebuffer>(framebuffer);

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vulkanPipeline->GetRenderPass();
	renderPassInfo.framebuffer = vulkanFramebuffer->GetFramebuffers()[m_RenderAPI->GetCurrentFrame().swapChainImageIndex];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = m_RenderContext->GetSwapChainExtent();

	VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->GetPipeline());
}

void VulkanCommandQueue::UnbindPipeline()
{
	vkCmdEndRenderPass(m_CommandBuffers[m_RenderContext->GetCurrentFrame()]);
}

void VulkanCommandQueue::BindVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer)
{
	VkBuffer vertexBuffers[] = { VertexBuffer::Get<VulkanVertexBuffer>(vertexBuffer)->GetBuffer() };
	VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], 0, 1, vertexBuffers, offsets);
}

void VulkanCommandQueue::BindIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer)
{
	auto buffer = IndexBuffer::Get<VulkanIndexBuffer>(indexBuffer)->GetBuffer();
	vkCmdBindIndexBuffer(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], buffer, 0, VK_INDEX_TYPE_UINT16);
}

void VulkanCommandQueue::SetViewport()
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(m_RenderContext->GetSwapChainExtent().width);
	viewport.height = static_cast<float>(m_RenderContext->GetSwapChainExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], 0, 1, &viewport);
}

void VulkanCommandQueue::SetScissor()
{
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_RenderContext->GetSwapChainExtent();
	vkCmdSetScissor(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], 0, 1, &scissor);
}

void VulkanCommandQueue::Draw()
{
	vkCmdDraw(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], 3, 1, 0, 0);
}

void VulkanCommandQueue::DrawIndexed(const std::shared_ptr<IndexBuffer>& indexBuffer)
{
	vkCmdDrawIndexed(m_CommandBuffers[m_RenderContext->GetCurrentFrame()], (uint32_t)indexBuffer->GetNumIndices(), 1, 0, 0, 0);
}
