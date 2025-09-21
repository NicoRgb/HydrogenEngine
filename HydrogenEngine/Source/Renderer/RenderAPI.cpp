#include "Hydrogen/Renderer/RenderAPI.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderAPI.hpp"

using namespace Hydrogen;

std::shared_ptr<RenderAPI> RenderAPI::Create(const std::shared_ptr<RenderContext>& renderContext)
{
	return std::make_shared<VulkanRenderAPI>(renderContext);
}
