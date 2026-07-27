#pragma once

#include "Hydrogen/Renderer/RenderDevice.hpp"
#include <vma/vk_mem_alloc.h>

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
		bool persistantMapping = false;
		bool dynamic = false;
	};

	class RenderBuffer
	{
	public:
		RenderBuffer(RenderDevice* device, const BufferDescription& desc);
		~RenderBuffer();

		RenderBuffer(const RenderBuffer&) = delete;
		RenderBuffer& operator=(const RenderBuffer&) = delete;

		void UploadData(const void* data, uint64_t size, uint64_t offset = 0);
		void UploadDataStaging(const void* data, uint64_t size, uint64_t offset = 0);

		void SetData(const void* data, uint64_t size);

		VkBuffer GetBuffer() const { return m_Buffer; }
		uint64_t GetSize() const { return m_Size; }
		uint64_t GetCapacity() const { return m_Capacity; }
		bool IsDynamic() const { return m_IsDynamic; }

	private:
		void CreateBuffer(uint64_t capacity);
		void DestroyBuffer();
		void ResizeBuffer(uint64_t newCapacity);

		RenderDevice* m_Device;
		BufferType m_Type;

		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VmaAllocation m_Allocation = VK_NULL_HANDLE;

		uint64_t m_Size = 0;
		uint64_t m_Capacity = 0;
		bool m_IsCpuVisible = false;
		bool m_PersistantMapping = false;
		bool m_IsDynamic = false;
		void* m_MappedData = nullptr;

		static constexpr float GROWTH_FACTOR = 1.5f;
	};
}
