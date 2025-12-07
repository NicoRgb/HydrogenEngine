#pragma once

#include "Hydrogen/Renderer/VertexBuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanBuffer
	{
	public:
		VulkanBuffer(const std::shared_ptr<VulkanRenderContext>& renderContext, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags propertyFlags);
		~VulkanBuffer();

		VulkanBuffer(const VulkanBuffer&) = delete;
		VulkanBuffer& operator=(const VulkanBuffer&) = delete;

		VulkanBuffer(VulkanBuffer&& other) noexcept
			: m_RenderContext(other.m_RenderContext),
			m_Buffer(other.m_Buffer),
			m_BufferMemory(other.m_BufferMemory),
			m_PropertyFlags(other.m_PropertyFlags),
			m_Size(other.m_Size)
		{
			other.m_Buffer = VK_NULL_HANDLE;
			other.m_BufferMemory = VK_NULL_HANDLE;
		}

		VulkanBuffer& operator=(VulkanBuffer&& other) noexcept
		{
			if (this != &other)
			{
				m_RenderContext = other.m_RenderContext;
				m_Buffer = other.m_Buffer;
				m_BufferMemory = other.m_BufferMemory;
				m_PropertyFlags = other.m_PropertyFlags;

				other.m_Buffer = VK_NULL_HANDLE;
				other.m_BufferMemory = VK_NULL_HANDLE;
			}

			return *this;
		}

		const VkBuffer GetBuffer() const { return m_Buffer; }
		const VkDeviceMemory GetBufferMemory() const { return m_BufferMemory; }

		const size_t GetSize() const { return m_Size; }

		void AllocateMemory();
		void UploadBufferData(void* data, size_t size);

		void CreateBuffer(VkDeviceSize size);
		void DestroyBuffer();

	protected:

		VkBuffer m_Buffer;
		VkDeviceMemory m_BufferMemory = nullptr;
		size_t m_Size = 0;

	private:
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		VkMemoryPropertyFlags m_PropertyFlags;
		VkBufferUsageFlags m_UsageFlags;

		std::shared_ptr<VulkanRenderContext> m_RenderContext;

		friend class VulkanVertexBuffer;
		friend class VulkanIndexBuffer;
	};

	class VulkanVertexBuffer : public VertexBuffer, public VulkanBuffer
	{
	public:
		VulkanVertexBuffer(const std::shared_ptr<RenderContext>& renderContext, VertexLayout layout, void* vertexData, size_t numVertices);
		~VulkanVertexBuffer();

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
	};

	class VulkanDynamicVertexBuffer : public DynamicVertexBuffer, public VulkanBuffer
	{
	public:
		VulkanDynamicVertexBuffer(const std::shared_ptr<RenderContext>& renderContext, VertexLayout layout, size_t numVertices);
		~VulkanDynamicVertexBuffer();

		void Resize(size_t numVertices) override;
		void Upload(void* data, size_t numVertices) override;

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		VertexLayout m_Layout;
		void* m_MappedPtr;
	};
}
