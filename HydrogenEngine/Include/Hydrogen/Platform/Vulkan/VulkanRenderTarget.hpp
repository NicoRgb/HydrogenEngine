#pragma once

#include "Hydrogen/Renderer/RenderTarget.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanRenderTarget : public RenderTarget
	{
	public:
		VulkanRenderTarget(const std::shared_ptr<RenderContext>& renderContext,
		                   const RenderTargetSpec& spec);
		~VulkanRenderTarget();

		uint32_t GetWidth() const override { return m_Spec.Width; }
		uint32_t GetHeight() const override { return m_Spec.Height; }
		const RenderTargetSpec& GetSpec() const override { return m_Spec; }

		VkImage GetColorImage() const { return m_ColorImage ? m_ColorImage->GetImage() : VK_NULL_HANDLE; }
		VkImageView GetColorImageView() const { return m_ColorImage ? m_ColorImage->GetImageView() : VK_NULL_HANDLE; }
		
		VkImage GetResolveImage() const { return m_ResolveImage ? m_ResolveImage->GetImage() : VK_NULL_HANDLE; }
		VkImageView GetResolveImageView() const { return m_ResolveImage ? m_ResolveImage->GetImageView() : VK_NULL_HANDLE; }
		
		VkImage GetDepthImage() const { return m_DepthImage ? m_DepthImage->GetImage() : VK_NULL_HANDLE; }
		VkImageView GetDepthImageView() const { return m_DepthImage ? m_DepthImage->GetImageView() : VK_NULL_HANDLE; }

		bool IsMultisampled() const override { return m_SampleCount > 1; }
		uint32_t GetSampleCount() const override { return m_SampleCount; }

		void OnResize(uint32_t width, uint32_t height) override;
		void Invalidate() override;

		VkRenderPass GetRenderPass() const { return m_RenderPass; }
		VkFramebuffer GetFramebuffer(uint32_t index = 0) const { return m_Framebuffers[index]; }
		const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_Framebuffers; }

		const std::shared_ptr<Texture> GetColorTexture() const override { return m_SampleCount == 1 ? m_ColorImage : m_ResolveImage; }

	private:
		void CreateAttachments();
		void CreateRenderPass();
		void CreateFramebuffers();
		void CreateSwapChainFramebuffers();
		void CreateTextureFramebuffers();
		void DestroyAttachments();

		std::shared_ptr<VulkanRenderContext> m_RenderContext;
		RenderTargetSpec m_Spec;
		uint32_t m_SampleCount;

		std::shared_ptr<VulkanTexture> m_ColorImage;
		std::shared_ptr<VulkanTexture> m_ResolveImage;
		std::shared_ptr<VulkanTexture> m_DepthImage;

		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		std::vector<VkFramebuffer> m_Framebuffers;
	};
}