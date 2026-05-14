#pragma once

#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	VkSampleCountFlagBits GetVulkanSampleCount(uint32_t sampleCount);

	class VulkanRenderGraph : public RenderGraph
	{
	public:
		VulkanRenderGraph(const std::shared_ptr<RenderContext>& renderContext,
		                   const RenderGraphSpec& spec);
		~VulkanRenderGraph();

		uint32_t GetWidth() const override { return m_Spec.Width; }
		uint32_t GetHeight() const override { return m_Spec.Height; }
		const RenderGraphSpec& GetSpec() const override { return m_Spec; }

		VkImage GetColorImage(uint32_t index) const { return m_ColorImages[index] ? m_ColorImages[index]->GetImage() : VK_NULL_HANDLE; }
		VkImageView GetColorImageView(uint32_t index) const { return m_ColorImages[index] ? m_ColorImages[index]->GetImageView() : VK_NULL_HANDLE; }
		
		VkImage GetResolveImage(uint32_t index) const { return m_ResolveImages[index] ? m_ResolveImages[index]->GetImage() : VK_NULL_HANDLE; }
		VkImageView GetResolveImageView(uint32_t index) const { return m_ResolveImages[index] ? m_ResolveImages[index]->GetImageView() : VK_NULL_HANDLE; }
		
		VkImage GetDepthImage() const { return m_DepthImage ? m_DepthImage->GetImage() : VK_NULL_HANDLE; }
		VkImageView GetDepthImageView() const { return m_DepthImage ? m_DepthImage->GetImageView() : VK_NULL_HANDLE; }

		uint32_t GetNumColorAttachments() const { return m_NumColorAttachments; }

		std::shared_ptr<Texture> GetColorTexture(uint32_t index) const override { return m_ColorImages[index]; }
		std::shared_ptr<Texture> GetDepthTexture() const override { return m_DepthImage; }
		std::shared_ptr<Texture> GetResolveTexture(uint32_t index) const override { return m_ResolveImages[index]; }

		void OnResize(uint32_t width, uint32_t height) override;
		void Invalidate() override;

		VkRenderPass GetRenderPass() const { return m_RenderPass; }
		VkFramebuffer GetFramebuffer(uint32_t index = 0) const { return m_Framebuffers[index]; }
		const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_Framebuffers; }

		uint32_t GetSampleCount() const { return m_SampleCount; }
		bool IsMultisampled() const { return m_SampleCount > 1; }
		bool IsSwapChainBacked() const { return m_IsSwapChainBacked; }

	private:
		void CreateAttachments();
		void CreateRenderPass();
		void CreateFramebuffers();
		void CreateSwapChainFramebuffers();
		void CreateTextureFramebuffers();
		void DestroyAttachments();

		std::shared_ptr<VulkanRenderContext> m_RenderContext;
		RenderGraphSpec m_Spec;

		std::vector<std::shared_ptr<VulkanTexture>> m_ColorImages;
		std::vector<std::shared_ptr<VulkanTexture>> m_ResolveImages;
		std::shared_ptr<VulkanTexture> m_DepthImage;

		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		std::vector<VkFramebuffer> m_Framebuffers;

		uint32_t m_SampleCount = false;
		bool m_IsSwapChainBacked = false;
		uint32_t m_NumColorAttachments = 0;
	};
}
