#pragma once

#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

namespace Hydrogen
{
	class VulkanPipeline : public Pipeline
	{
	public:
		VulkanPipeline(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<ShaderAsset>& vertexShaderAsset, const std::shared_ptr<ShaderAsset>& fragmentShaderAsset, VertexLayout vertexLayout);
		~VulkanPipeline();

		const VkPipeline GetPipeline() const { return m_Pipeline; }

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_Pipeline;

		VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code) const;
	};
}
