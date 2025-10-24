#pragma once

#include "Hydrogen/Renderer/Framebuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderPass.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanFramebuffer : public Framebuffer
	{
	public:
		VulkanFramebuffer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<Texture>& texture);
		~VulkanFramebuffer();

		bool RenderToTexture() { return m_Texture != nullptr; }
		const std::shared_ptr<Texture> GetTexture() override { return m_Texture; }

		void OnResize(int width, int height) override;

		const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_Framebuffers; }

	private:
		void CreateFramebuffers();

		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
		const std::shared_ptr<VulkanRenderPass> m_RenderPass;
		const std::shared_ptr<VulkanTexture> m_Texture;

		std::vector<VkFramebuffer> m_Framebuffers;

		VkImage m_DepthImage;
		VkDeviceMemory m_DepthImageMemory;
		VkImageView m_DepthImageView;
	};
}
