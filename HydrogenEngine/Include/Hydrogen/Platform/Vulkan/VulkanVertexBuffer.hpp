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

		const VkBuffer GetBuffer() const { return m_Buffer; }

	protected:
		void AllocateMemory();
		void UploadBufferData(void* data, size_t size);

		VkBuffer m_Buffer;
		VkDeviceMemory m_BufferMemory = nullptr;

	private:
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		VkMemoryPropertyFlags m_PropertyFlags;

		const std::shared_ptr<VulkanRenderContext> m_RenderContext;

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
}
