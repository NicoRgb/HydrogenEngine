#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

using namespace Hydrogen;

std::shared_ptr<RenderContext> RenderContext::Create(std::string appName, glm::vec2 appVersion)
{
	return std::make_shared<VulkanRenderContext>(appName, appVersion);
}
