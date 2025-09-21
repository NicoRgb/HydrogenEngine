#include "Hydrogen/Renderer/CommandQueue.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanCommandQueue.hpp"

using namespace Hydrogen;

std::shared_ptr<CommandQueue> CommandQueue::Create(const std::shared_ptr<RenderContext>& renderContext)
{
	return std::make_shared<VulkanCommandQueue>(renderContext);
}
