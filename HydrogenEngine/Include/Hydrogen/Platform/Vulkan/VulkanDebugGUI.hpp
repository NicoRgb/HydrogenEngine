#pragma once

#include "Hydrogen/Renderer/DebugGUI.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

#include "imgui.h"

namespace Hydrogen
{
	class VulkanDebugGUI : public DebugGUI
	{
	public:
		VulkanDebugGUI(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<FrameGraph>& frameGraph, std::string framePass);
		~VulkanDebugGUI();

		void BeginFrame() override;
		void EndFrame() override;
		void Render(const std::shared_ptr<CommandBuffer>& commandBuffer) override;

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;

		VkDescriptorPool m_ImguiPool;
		ImDrawData* main_draw_data = nullptr;
	};
}
