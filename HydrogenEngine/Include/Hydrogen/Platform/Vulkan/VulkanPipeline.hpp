#pragma once

#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanVertexBuffer.hpp"

namespace Hydrogen
{
	class VulkanPipeline : public Pipeline
	{
	public:
		VulkanPipeline(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<ShaderAsset>& vertexShaderAsset, const std::shared_ptr<ShaderAsset>& fragmentShaderAsset, VertexLayout vertexLayout, const std::vector<DescriptorBinding> descriptorBindings);
		~VulkanPipeline();

		void UploadUniformBufferData(uint32_t binding, void* data, size_t size) override;

		const VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
		const VkPipeline GetPipeline() const { return m_Pipeline; }
		const std::vector<VkDescriptorSet> GetDescriptorSets() const { return m_DescriptorSets; }

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		VkDescriptorSetLayout m_DescriptorSetLayout;
		VkDescriptorPool m_DescriptorPool;
		std::vector<VkDescriptorSet> m_DescriptorSets;
		VkPipelineLayout m_PipelineLayout;
		VkPipeline m_Pipeline;

		VkShaderModule CreateShaderModule(const std::vector<uint32_t>& code) const;

		std::map<uint32_t, std::vector<VulkanBuffer>> m_UniformBuffers;
		std::map<uint32_t, std::vector<void*>> m_UniformBuffersMapped;
	};
}
