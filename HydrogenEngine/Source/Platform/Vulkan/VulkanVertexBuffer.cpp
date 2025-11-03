#include "Hydrogen/Platform/Vulkan/VulkanVertexBuffer.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

VulkanBuffer::VulkanBuffer(const std::shared_ptr<VulkanRenderContext>& renderContext, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags)
	: m_RenderContext(renderContext), m_PropertyFlags(propertyFlags), m_Size(size)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usageFlags;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	HY_ASSERT(vkCreateBuffer(m_RenderContext->GetDevice(), &bufferInfo, nullptr, &m_Buffer) == VK_SUCCESS, "Failed to create vulkan buffer");
}

VulkanBuffer::~VulkanBuffer()
{
	vkDestroyBuffer(m_RenderContext->GetDevice(), m_Buffer, nullptr);
	if (m_BufferMemory != nullptr)
	{
		vkFreeMemory(m_RenderContext->GetDevice(), m_BufferMemory, nullptr);
	}
}

void VulkanBuffer::AllocateMemory()
{
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(m_RenderContext->GetDevice(), m_Buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, m_PropertyFlags);

	HY_ASSERT(vkAllocateMemory(m_RenderContext->GetDevice(), &allocInfo, nullptr, &m_BufferMemory) == VK_SUCCESS, "Failed to allocate vulkan buffer memory");

	vkBindBufferMemory(m_RenderContext->GetDevice(), m_Buffer, m_BufferMemory, 0);
}

void VulkanBuffer::UploadBufferData(void* data, size_t size)
{
	void* mem;
	HY_ASSERT(vkMapMemory(m_RenderContext->GetDevice(), m_BufferMemory, 0, (VkDeviceSize)size, 0, &mem) == VK_SUCCESS, "Failed to map memory");
	memcpy(mem, data, size);
	vkUnmapMemory(m_RenderContext->GetDevice(), m_BufferMemory);
}

uint32_t VulkanBuffer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(m_RenderContext->GetPhysicalDevice(), &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	HY_ASSERT(false, "Failed to find suitable buffer memory type");
	return 0;
}

VulkanVertexBuffer::VulkanVertexBuffer(const std::shared_ptr<RenderContext>& renderContext, VertexLayout layout, void* vertexData, size_t numVertices)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext)),
	VulkanBuffer(RenderContext::Get<VulkanRenderContext>(renderContext), GetVertexSize(layout) * numVertices, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
{
	size_t dataSize = GetVertexSize(layout) * numVertices;

	VulkanBuffer stagingBuffer(m_RenderContext, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingBuffer.AllocateMemory();
	stagingBuffer.UploadBufferData(vertexData, dataSize);

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

VulkanVertexBuffer::~VulkanVertexBuffer()
{
}
