#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include "Hydrogen/Core.hpp"

#ifdef HY_WITH_IMGUI
#include "backends/imgui_impl_vulkan.h"
#endif

using namespace Hydrogen;

static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) &&
			(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	HY_ASSERT(false, "Failed to find suitable memory type");
	return 0;
}

VulkanTexture::VulkanTexture(const std::shared_ptr<RenderContext>& renderContext, TextureFormat format, size_t width, size_t height)
	: m_RenderContext(RenderContext::Get<VulkanRenderContext>(renderContext))
{
	m_Format = format;
	m_Width = width;
	m_Height = height;

	CreateTexture();
}

VulkanTexture::~VulkanTexture()
{
	vkFreeMemory(m_RenderContext->GetDevice(), m_ImageMemory, nullptr);
	vkDestroySampler(m_RenderContext->GetDevice(), m_Sampler, nullptr);
	vkDestroyImageView(m_RenderContext->GetDevice(), m_ImageView, nullptr);
	vkDestroyImage(m_RenderContext->GetDevice(), m_Image, nullptr);
}

ImTextureID VulkanTexture::GetImGuiImage()
{
#ifdef HY_WITH_IMGUI
	if (m_ImGuiImage == 0)
	{
		m_ImGuiImage = (ImTextureID)ImGui_ImplVulkan_AddTexture(
			m_Sampler,
			m_ImageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

	return m_ImGuiImage;
#else
	return 0;
#endif
}

void VulkanTexture::Resize(size_t width, size_t height)
{
	m_Width = width;
	m_Height = height;

	vkDeviceWaitIdle(m_RenderContext->GetDevice());

#ifdef HY_WITH_IMGUI
if (m_ImGuiImage != 0)
{
	ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_ImGuiImage);
	m_ImGuiImage = 0;
}
#endif

	vkFreeMemory(m_RenderContext->GetDevice(), m_ImageMemory, nullptr);
	vkDestroySampler(m_RenderContext->GetDevice(), m_Sampler, nullptr);
	vkDestroyImageView(m_RenderContext->GetDevice(), m_ImageView, nullptr);
	vkDestroyImage(m_RenderContext->GetDevice(), m_Image, nullptr);

	CreateTexture();
}

void VulkanTexture::CreateTexture()
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = m_RenderContext->GetSwapChainImageFormat();
	imageInfo.extent.width = (uint32_t)m_Width;
	imageInfo.extent.height = (uint32_t)m_Height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	HY_ASSERT(vkCreateImage(m_RenderContext->GetDevice(), &imageInfo, nullptr, &m_Image) == VK_SUCCESS, "Failed to create vulkan view");

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_RenderContext->GetDevice(), m_Image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(
		memRequirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		m_RenderContext->GetPhysicalDevice());

	HY_ASSERT(vkAllocateMemory(m_RenderContext->GetDevice(), &allocInfo, nullptr, &m_ImageMemory) == VK_SUCCESS, "Failed to allocate vulkan memory");

	HY_ASSERT(vkBindImageMemory(m_RenderContext->GetDevice(), m_Image, m_ImageMemory, 0) == VK_SUCCESS, "Failed to bind vulkan memory");

	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = m_Image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = m_RenderContext->GetSwapChainImageFormat();
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;

	HY_ASSERT(vkCreateImageView(m_RenderContext->GetDevice(), &createInfo, nullptr, &m_ImageView) == VK_SUCCESS, "Failed to create vulkan image view");

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	vkCreateSampler(m_RenderContext->GetDevice(), &samplerInfo, nullptr, &m_Sampler);
}
