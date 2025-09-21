#include "Hydrogen/Platform/Vulkan/VulkanCommandQueue.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanFramebuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderAPI.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

VulkanCommandQueue::VulkanCommandQueue(const std::shared_ptr<RenderContext>& renderContext)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext))
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_RenderContext->GetCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	HY_ASSERT(vkAllocateCommandBuffers(m_RenderContext->GetDevice(), &allocInfo, &m_CommandBuffer) == VK_SUCCESS, "Failed to allocate vulkan command buffer");
}

VulkanCommandQueue::~VulkanCommandQueue()
{
}

void VulkanCommandQueue::StartRecording(const std::shared_ptr<RenderAPI>& renderAPI)
{
	m_RenderAPI = RenderAPI::Get<VulkanRenderAPI>(renderAPI);

	vkResetCommandBuffer(m_CommandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	HY_ASSERT(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) == VK_SUCCESS, "Failed to begin recording vulkan command buffer");
}

void VulkanCommandQueue::EndRecording()
{
	m_RenderAPI = nullptr;
	HY_ASSERT(vkEndCommandBuffer(m_CommandBuffer) == VK_SUCCESS, "Failed to record vulkan command buffer");
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

	vkCmdBeginRenderPass(m_CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipeline->GetPipeline());
}

void VulkanCommandQueue::UnbindPipeline()
{
	vkCmdEndRenderPass(m_CommandBuffer);
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
	vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);
}

void VulkanCommandQueue::SetScissor()
{
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_RenderContext->GetSwapChainExtent();
	vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
}

void VulkanCommandQueue::Draw()
{
	vkCmdDraw(m_CommandBuffer, 3, 1, 0, 0);
}
