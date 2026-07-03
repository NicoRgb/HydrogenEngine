#include "Hydrogen/Renderer/RenderBuffer.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

RenderBuffer::RenderBuffer(RenderDevice* device, const BufferDescription& desc)
    : m_Device(device), m_Size(desc.size), m_IsCpuVisible(desc.cpuVisible)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = m_Size;

    switch (desc.type)
    {
        case BufferType::Vertex:  bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT; break;
        case BufferType::Index:   bufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT; break;
        case BufferType::Uniform: bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
        case BufferType::Storage: bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT; break;
        case BufferType::Staging: bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT; break;
    }
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(m_Device->GetVulkanDevice(), &bufferInfo, nullptr, &m_Buffer);
    if (result != VK_SUCCESS)
    {
        HY_ENGINE_FATAL("Failed to create Vulkan buffer... vkCreateBuffer returned {}", (uint16_t)result);
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_Device->GetVulkanDevice(), m_Buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;

    VkMemoryPropertyFlags properties = desc.cpuVisible
        ? (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    allocInfo.memoryTypeIndex = FindMemoryType(m_Device->GetVulkanPhysicalDevice(), memRequirements.memoryTypeBits, properties);

    result = vkAllocateMemory(m_Device->GetVulkanDevice(), &allocInfo, nullptr, &m_Memory);
    if (result != VK_SUCCESS)
    {
        HY_ENGINE_FATAL("Failed to allocate Vulkan buffer memory... vkAllocateMemory returned {}", (uint16_t)result);
    }

    vkBindBufferMemory(m_Device->GetVulkanDevice(), m_Buffer, m_Memory, 0);
}

RenderBuffer::~RenderBuffer()
{
    if (m_Buffer != VK_NULL_HANDLE) vkDestroyBuffer(m_Device->GetVulkanDevice(), m_Buffer, nullptr);
    if (m_Memory != VK_NULL_HANDLE) vkFreeMemory(m_Device->GetVulkanDevice(), m_Memory, nullptr);
}

void RenderBuffer::UploadData(const void* data, uint64_t size, uint64_t offset)
{
    HY_ASSERT(m_IsCpuVisible, "Cannot directly upload data to a non-CPU visible buffer! Use a staging buffer.");

    void* mappedData;
    vkMapMemory(m_Device->GetVulkanDevice(), m_Memory, offset, size, 0, &mappedData);
    std::memcpy(mappedData, data, size);
    vkUnmapMemory(m_Device->GetVulkanDevice(), m_Memory);
}

uint32_t RenderBuffer::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    HY_ASSERT(false, "Failed to find suitable memory type!");
}
