#include "Hydrogen/Platform/Vulkan/VulkanIndexBuffer.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

VulkanIndexBuffer::VulkanIndexBuffer(const std::shared_ptr<RenderContext>& renderContext, const std::vector<uint16_t>& indices)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)),
	VulkanBuffer(RenderContext::Get<VulkanRenderContext>(renderContext), sizeof(indices[0]) * indices.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
{
	m_NumIndices = indices.size();
	size_t dataSize = sizeof(indices[0]) * indices.size();

	VulkanBuffer stagingBuffer(m_RenderContext, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingBuffer.AllocateMemory();
	stagingBuffer.UploadBufferData((void*)indices.data(), dataSize);

	AllocateMemory();

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_RenderContext->GetCommandPool();
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_RenderContext->GetDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = dataSize;
	vkCmdCopyBuffer(commandBuffer, stagingBuffer.GetBuffer(), m_Buffer, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(m_RenderContext->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_RenderContext->GetGraphicsQueue());

	vkFreeCommandBuffers(m_RenderContext->GetDevice(), m_RenderContext->GetCommandPool(), 1, &commandBuffer);
}

VulkanIndexBuffer::~VulkanIndexBuffer()
{
}
