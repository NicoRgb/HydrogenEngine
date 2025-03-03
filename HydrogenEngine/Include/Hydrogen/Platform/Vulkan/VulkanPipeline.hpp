#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

namespace Hydrogen
{
	class VulkanPipeline : public Pipeline
	{
	public:
		VulkanPipeline(const std::shared_ptr<RenderContext>& renderContext, const std::string& vertexShaderSrc, const std::string& fragmentShaderSrc);
		~VulkanPipeline();

		const VkRenderPass GetRenderPass() { return m_RenderPass; }

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		VkRenderPass m_RenderPass;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_Pipeline;

		VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code) const;
	};
}
