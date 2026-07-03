#pragma once

#include "Hydrogen/Renderer/RenderDevice.hpp"

namespace Hydrogen
{
    enum class BufferType
    {
        Vertex,
        Index,
        Uniform,
        Storage,
        Staging
    };

    struct BufferDescription
    {
        uint64_t size = 0;
        BufferType type = BufferType::Uniform;
        bool cpuVisible = false;
    };

    class RenderBuffer {
    public:
        RenderBuffer(RenderDevice* device, const BufferDescription& desc);
        ~RenderBuffer();

        RenderBuffer(const RenderBuffer&) = delete;
        RenderBuffer& operator=(const RenderBuffer&) = delete;

        void UploadData(const void* data, uint64_t size, uint64_t offset = 0);

        VkBuffer GetHandle() const { return m_Buffer; }
        uint64_t GetSize() const { return m_Size; }

    private:
        uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

        RenderDevice* m_Device;

        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_Memory = VK_NULL_HANDLE;

        uint64_t m_Size = 0;
        bool m_IsCpuVisible = false;
    };
}
