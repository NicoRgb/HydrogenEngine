#pragma once

#include "Hydrogen/Renderer/IndexBuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanVertexBuffer.hpp"

#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanIndexBuffer : public IndexBuffer, public VulkanBuffer
	{
	public:
		VulkanIndexBuffer(const std::shared_ptr<RenderContext>& renderContext, const std::vector<uint16_t>& indices);
		~VulkanIndexBuffer();

		const size_t GetNumIndices() const override { return m_NumIndices; }

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;

		size_t m_NumIndices;
	};
}
