#include "Hydrogen/Renderer/CommandBuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanCommandBuffer.hpp"

using namespace Hydrogen;

std::shared_ptr<CommandBuffer> CommandBuffer::Create(const std::shared_ptr<RenderContext>& renderContext)
{
	return std::make_shared<VulkanCommandBuffer>(renderContext);
}
