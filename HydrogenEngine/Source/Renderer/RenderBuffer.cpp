#include "Hydrogen/Renderer/RenderBuffer.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Core.hpp"
#include <vma/vk_mem_alloc.h>

using namespace Hydrogen;

RenderBuffer::RenderBuffer(RenderDevice* device, const BufferDescription& desc)
	: m_Device(device), m_Type(desc.type), m_Size(desc.size), m_Capacity(desc.size),
	m_IsCpuVisible(desc.cpuVisible), m_PersistantMapping(desc.persistantMapping),
	m_IsDynamic(desc.dynamic)
{
	CreateBuffer(desc.size);
}

RenderBuffer::~RenderBuffer()
{
	DestroyBuffer();
}

void RenderBuffer::CreateBuffer(uint64_t capacity)
{
	HY_ASSERT(capacity > 0, "Buffer capacity must be greater than 0!");

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = capacity;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	switch (m_Type)
	{
	case BufferType::Vertex:
		bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		break;
	case BufferType::Index:
		bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		break;
	case BufferType::Uniform:
		bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		break;
	case BufferType::Storage:
		bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		break;
	case BufferType::Staging:
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		break;
	default:
		HY_ENGINE_FATAL("Unknown buffer type!");
		break;
	}

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

	if (m_IsCpuVisible)
	{
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
		if (m_PersistantMapping)
		{
			allocInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
	}

	VmaAllocator allocator = m_Device->GetAllocator();
	VkResult result = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &m_Buffer, &m_Allocation, nullptr);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create VMA buffer... vmaCreateBuffer returned {}", (uint16_t)result);
	}

	if (m_PersistantMapping)
	{
		result = vmaMapMemory(allocator, m_Allocation, &m_MappedData);
		if (result != VK_SUCCESS)
		{
			HY_ENGINE_FATAL("Failed to map buffer memory... vmaMapMemory returned {}", (uint16_t)result);
		}
	}

	m_Capacity = capacity;
}

void RenderBuffer::DestroyBuffer()
{
	if (m_Device == nullptr) return;

	VmaAllocator allocator = m_Device->GetAllocator();

	if (m_MappedData && m_PersistantMapping && allocator)
	{
		vmaUnmapMemory(allocator, m_Allocation);
		m_MappedData = nullptr;
	}

	if (m_Buffer != VK_NULL_HANDLE && allocator)
	{
		vmaDestroyBuffer(allocator, m_Buffer, m_Allocation);
		m_Buffer = VK_NULL_HANDLE;
		m_Allocation = VK_NULL_HANDLE;
	}
}

void RenderBuffer::ResizeBuffer(uint64_t newCapacity)
{
	HY_ASSERT(m_IsDynamic, "Cannot resize a non-dynamic buffer!");
	HY_ENGINE_WARN("Dynamic buffer resizing from {} to {} bytes", m_Capacity, newCapacity);

	VmaAllocator allocator = m_Device->GetAllocator();

	std::vector<uint8_t> oldData;
	if (m_Size > 0)
	{
		oldData.resize(m_Size);

		if (m_PersistantMapping && m_MappedData)
		{
			std::memcpy(oldData.data(), m_MappedData, m_Size);
		}
		else
		{
			void* tempMappedData;
			vmaMapMemory(allocator, m_Allocation, &tempMappedData);
			std::memcpy(oldData.data(), tempMappedData, m_Size);
			vmaUnmapMemory(allocator, m_Allocation);
		}
	}

	DestroyBuffer();
	CreateBuffer(newCapacity);

	if (!oldData.empty())
	{
		if (m_PersistantMapping)
		{
			std::memcpy(m_MappedData, oldData.data(), oldData.size());
		}
		else
		{
			void* tempMappedData;
			vmaMapMemory(allocator, m_Allocation, &tempMappedData);
			std::memcpy(tempMappedData, oldData.data(), oldData.size());
			vmaUnmapMemory(allocator, m_Allocation);
		}
	}
}

void RenderBuffer::UploadData(const void* data, uint64_t size, uint64_t offset)
{
	HY_ASSERT(m_IsCpuVisible, "Cannot directly upload data to a non-CPU visible buffer! Use a staging buffer.");
	HY_ASSERT(offset + size <= m_Capacity, "Data exceeds buffer capacity!");

	VmaAllocator allocator = m_Device->GetAllocator();

	if (m_PersistantMapping)
	{
		std::memcpy(static_cast<uint8_t*>(m_MappedData) + offset, data, size);
		vmaFlushAllocation(allocator, m_Allocation, offset, size);
		return;
	}

	void* mappedData;
	vmaMapMemory(allocator, m_Allocation, &mappedData);
	std::memcpy(static_cast<uint8_t*>(mappedData) + offset, data, size);
	vmaFlushAllocation(allocator, m_Allocation, offset, size);
	vmaUnmapMemory(allocator, m_Allocation);
}

void RenderBuffer::UploadDataStaging(const void* data, uint64_t size, uint64_t offset)
{
	BufferDescription stagingDesc{};
	stagingDesc.size = size;
	stagingDesc.type = BufferType::Staging;
	stagingDesc.cpuVisible = true;
	stagingDesc.persistantMapping = true;

	RenderBuffer stagingBuffer(m_Device, stagingDesc);
	stagingBuffer.UploadData(data, size);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_Device->GetCommandPool();
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_Device->GetVulkanDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = offset;
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, stagingBuffer.GetBuffer(), GetBuffer(), 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_Device->GetGraphicsQueue());

	vkFreeCommandBuffers(m_Device->GetVulkanDevice(), m_Device->GetCommandPool(), 1, &commandBuffer);
}

void RenderBuffer::SetData(const void* data, uint64_t size)
{
	HY_ASSERT(m_IsDynamic, "SetData is only for dynamic buffers! Use UploadData for static buffers.");
	HY_ASSERT(data && size > 0, "Invalid data or size!");

	if (size > m_Capacity)
	{
		uint64_t newCapacity = static_cast<uint64_t>(size * GROWTH_FACTOR);
		ResizeBuffer(newCapacity);
	}

	m_Size = size;
	UploadData(data, size, 0);
}
