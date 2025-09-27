#include "Hydrogen/Renderer/IndexBuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanIndexBuffer.hpp"

using namespace Hydrogen;

std::shared_ptr<IndexBuffer> IndexBuffer::Create(const std::shared_ptr<RenderContext>& renderContext, const std::vector<uint16_t>& indices)
{
	return std::make_shared<VulkanIndexBuffer>(renderContext, indices);
}
