#pragma once

#include "Hydrogen/Renderer/Framebuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanPipeline.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Pipeline>& pipeline, bool renderToTexture);
		~VulkanFramebuffer();

		void CreateFramebuffers();

		void OnResize(int width, int height) override;

		const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_Framebuffers; }
		const std::vector<VkDescriptorSet>& GetImGuiTextures() const { return m_ImguiTextures; }

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		const std::shared_ptr<VulkanPipeline> m_Pipeline;
		const bool m_RenderToTexture;

		std::vector<VkFramebuffer> m_Framebuffers;

		std::vector<VkImageView> m_ImageViews;
		std::vector<VkImage> m_Images;
		std::vector<VkSampler> m_Samplers;
		std::vector<VkDescriptorSet> m_ImguiTextures;
	};
}
