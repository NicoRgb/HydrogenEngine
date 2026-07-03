#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Core.hpp"

#include <backends/imgui_impl_vulkan.h>

using namespace Hydrogen;

Texture::Texture(RenderDevice* device, const TextureDescription& desc)
	: m_Device(device), m_Desc(desc)
{

	VkFormat vkFormat = VK_FORMAT_R8G8B8A8_SRGB;
	VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	switch (desc.format)
	{
	case TextureFormat::RGBA8_SRGB:    vkFormat = VK_FORMAT_R8G8B8A8_SRGB; break;
	case TextureFormat::RGBA16_SFLOAT: vkFormat = VK_FORMAT_R16G16B16A16_SFLOAT; break;
	case TextureFormat::BGRA8_SRGB:    vkFormat = VK_FORMAT_B8G8R8A8_SRGB; break;
	case TextureFormat::D32_SFLOAT:
		vkFormat = VK_FORMAT_D32_SFLOAT;
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	}

	VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (((uint32_t)desc.usageFlags & (uint32_t)TextureUsage::SampledImage) == (uint32_t)TextureUsage::SampledImage)
	{
		usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if (((uint32_t)desc.usageFlags & (uint32_t)TextureUsage::ColorAttachment) == (uint32_t)TextureUsage::ColorAttachment)
	{
		usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (((uint32_t)desc.usageFlags & (uint32_t)TextureUsage::DepthAttachment) == (uint32_t)TextureUsage::DepthAttachment)
	{
		usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = desc.width;
	imageInfo.extent.height = desc.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = vkFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = usageFlags;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateImage(m_Device->GetVulkanDevice(), &imageInfo, nullptr, &m_Image);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan image... vkCreateImage returned {}", (uint16_t)result);
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(m_Device->GetVulkanDevice(), m_Image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = FindMemoryType(m_Device->GetVulkanPhysicalDevice(), memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	result = vkAllocateMemory(m_Device->GetVulkanDevice(), &allocInfo, nullptr, &m_Memory);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to allocate Vulkan texture memory... vkAllocateMemory returned {}", (uint16_t)result);
	}

	vkBindImageMemory(m_Device->GetVulkanDevice(), m_Image, m_Memory, 0);

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_Image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = vkFormat;
	viewInfo.subresourceRange.aspectMask = aspectMask;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	result = vkCreateImageView(m_Device->GetVulkanDevice(), &viewInfo, nullptr, &m_ImageView);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan image view... vkCreateImageView returned {}", (uint16_t)result);
	}

	if (((uint32_t)desc.usageFlags & (uint32_t)TextureUsage::SampledImage) == (uint32_t)TextureUsage::SampledImage)
	{
		CreateSampler();
	}
}

Texture::Texture(RenderDevice* device, VkImage image, VkImageView imageView, const TextureDescription& desc, VkSampler sampler)
	: m_Device(device), m_Desc(desc), m_Image(image), m_ImageView(imageView), m_Sampler(sampler)
{
}

Texture::~Texture()
{
	if (m_ImGuiImage != 0) ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_ImGuiImage);
	if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(m_Device->GetVulkanDevice(), m_ImageView, nullptr);
	if (m_Image != VK_NULL_HANDLE) vkDestroyImage(m_Device->GetVulkanDevice(), m_Image, nullptr);
	if (m_Memory != VK_NULL_HANDLE) vkFreeMemory(m_Device->GetVulkanDevice(), m_Memory, nullptr);
}

ImTextureID Texture::GetImGuiTexture()
{
	if (m_ImGuiImage == 0)
	{
		m_ImGuiImage = (ImTextureID)ImGui_ImplVulkan_AddTexture(
			m_Sampler,
			m_ImageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		);
	}

	return m_ImGuiImage;
}

uint32_t Texture::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	HY_ASSERT(false, "Failed to find suitable memory type!");
}

void Texture::CreateSampler()
{
	VkSamplerCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	createInfo.magFilter = VK_FILTER_LINEAR;
	createInfo.minFilter = VK_FILTER_LINEAR;

	createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

	createInfo.anisotropyEnable = VK_FALSE;
	createInfo.maxAnisotropy = 1.0f;
	createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	createInfo.unnormalizedCoordinates = VK_FALSE;

	createInfo.compareEnable = VK_FALSE;
	createInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	createInfo.mipLodBias = 0.0f;
	createInfo.minLod = 0.0f;
	createInfo.maxLod = 1.0f;

	VkResult result = vkCreateSampler(m_Device->GetVulkanDevice(), &createInfo, nullptr, &m_Sampler);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan sampler... vkCreateSampler returned {}", (uint16_t)result);
	}
}
