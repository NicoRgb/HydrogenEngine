#include "Hydrogen/Renderer/RenderPipeline.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

namespace Hydrogen
{
	class VulkanRenderPipeline : public RenderPipeline
	{
	public:
		VulkanRenderPipeline(const std::shared_ptr<RenderContext>& renderContext, const std::string& vertexShaderSrc, const std::string& fragmentShaderSrc);
		~VulkanRenderPipeline();

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		VkRenderPass m_RenderPass;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_Pipeline;

		VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code) const;
	};
}
