#include "Hydrogen/Renderer/DebugGUI.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanDebugGUI.hpp"

using namespace Hydrogen;

std::shared_ptr<DebugGUI> DebugGUI::Create(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass)
{
#ifdef HY_WITH_IMGUI
	return std::make_shared<VulkanDebugGUI>(renderContext, renderPass);
#endif
	return nullptr;
}
