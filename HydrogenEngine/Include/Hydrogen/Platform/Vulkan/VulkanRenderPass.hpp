#pragma once

#include "Hydrogen/Renderer/RenderPass.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanRenderPass : public RenderPass
	{
	public:
		VulkanRenderPass(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Texture>& texture);
		~VulkanRenderPass();

		VkRenderPass GetRenderPass() const { return m_RenderPass; }

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;

		VkRenderPass m_RenderPass;
	};
}
