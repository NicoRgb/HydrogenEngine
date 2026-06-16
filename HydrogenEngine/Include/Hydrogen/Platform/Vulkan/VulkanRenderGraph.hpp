#pragma once

#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	VkSampleCountFlagBits GetVulkanSampleCount(uint32_t sampleCount);

	struct FrameAttachment
	{
		std::shared_ptr<VulkanTexture> Texture;
		VkImageLayout CurrentLayout;
		bool Cleared = false;
	};

	struct VulkanFramePass
	{
		VkRenderPass RenderPass;
		std::vector<VkFramebuffer> Framebuffers;
		
		uint32_t NumColorAttachments = 0;
		uint32_t NumClearedColorAttachments = 0;
		bool HasDepthAttachment = false;
		bool DepthCleared = false;

		uint32_t SampleCount = 1;
		bool IsSwapchainBacked = false;
	};

	class VulkanFrameGraph : public FrameGraph
	{
	public:
		VulkanFrameGraph(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height);
		~VulkanFrameGraph();

		void Compose() override;
		void Resize(uint32_t width, uint32_t height) override;
		uint32_t GetWidth() const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }

		std::shared_ptr<Texture> GetTexture(const std::string& resourceName) const override { return m_Attachments.at(resourceName).Texture; }

		const VulkanFramePass& GetVulkanFramePass(const std::string& name) const { return m_VkFramePasses.at(name); }

	private:
		void CreatePass(FramePass* pass, std::string name);
		void CreateAttachment(std::string name, const FrameAttachmentHandle& handle);
		void CreateRenderPass(std::string name, FramePass* pass);
		void CreateFramebuffers(std::string name, FramePass* pass);
		void CreateFramebuffersSwapchainBacked(std::string name, FramePass* pass);
		void CreatePipelines(std::string name, FramePass* pass);

		std::shared_ptr<VulkanRenderContext> m_RenderContext;
		uint32_t m_Width, m_Height;

		std::unordered_map<std::string, FrameAttachment> m_Attachments;
		std::unordered_map<std::string, VulkanFramePass> m_VkFramePasses;
	};
}
