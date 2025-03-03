#include "Hydrogen/Renderer/Framebuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanPipeline.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Pipeline>& pipeline);
		~VulkanFramebuffer();

	private:
		std::vector<VkFramebuffer> m_Framebuffers;
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		const std::shared_ptr<VulkanPipeline> m_Pipeline;
	};
}
