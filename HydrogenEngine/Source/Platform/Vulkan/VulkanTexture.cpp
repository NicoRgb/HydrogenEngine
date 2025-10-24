#include "Hydrogen/Platform/Vulkan/VulkanTexture.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanVertexBuffer.hpp"
#include "Hydrogen/Core.hpp"

#include "backends/imgui_impl_vulkan.h"

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
	m_Width = width;
	m_Height = height;

	switch (format)
	{
	case TextureFormat::ViewportDefault:
		m_Format = m_RenderContext->GetSwapChainImageFormat();
		break;
	case TextureFormat::FormatR8G8B8A8:
		m_Format = VK_FORMAT_R8G8B8A8_SRGB;
		break;
	default:
		HY_ASSERT(false, "Invalid TextureFormat");
	}

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

void VulkanTexture::Resize(size_t width, size_t height)
{
	m_Width = width;
	m_Height = height;

	vkDeviceWaitIdle(m_RenderContext->GetDevice());

	if (m_ImGuiImage != 0)
	{
		ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)m_ImGuiImage);
		m_ImGuiImage = 0;
	}

	vkFreeMemory(m_RenderContext->GetDevice(), m_ImageMemory, nullptr);
	vkDestroySampler(m_RenderContext->GetDevice(), m_Sampler, nullptr);
	vkDestroyImageView(m_RenderContext->GetDevice(), m_ImageView, nullptr);
	vkDestroyImage(m_RenderContext->GetDevice(), m_Image, nullptr);

	CreateTexture();
}

void VulkanTexture::UploadData(void* data)
{
	size_t size = m_Width * m_Height * sizeof(uint32_t);

	VulkanBuffer stagingBuffer(m_RenderContext, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	stagingBuffer.AllocateMemory();

	void* d;
	vkMapMemory(m_RenderContext->GetDevice(), stagingBuffer.GetBufferMemory(), 0, size, 0, &d);
	memcpy(d, data, size);
	vkUnmapMemory(m_RenderContext->GetDevice(), stagingBuffer.GetBufferMemory());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_RenderContext->GetCommandPool();
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_RenderContext->GetDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	{
		TransitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = {
			static_cast<uint32_t>(m_Width),
			static_cast<uint32_t>(m_Height),
			1
		};

		vkCmdCopyBufferToImage(
			commandBuffer,
			stagingBuffer.GetBuffer(),
			m_Image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region
		);

		TransitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(m_RenderContext->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_RenderContext->GetGraphicsQueue());

	vkFreeCommandBuffers(m_RenderContext->GetDevice(), m_RenderContext->GetCommandPool(), 1, &commandBuffer);
}

void VulkanTexture::TransitionImageLayout(VkCommandBuffer commandBuffer, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_Image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

void VulkanTexture::CreateTexture()
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = m_Format;
	imageInfo.extent.width = (uint32_t)m_Width;
	imageInfo.extent.height = (uint32_t)m_Height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // TODO: seperate for different usages
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
	createInfo.format = m_Format;
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

	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(m_RenderContext->GetPhysicalDevice(), &properties);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	vkCreateSampler(m_RenderContext->GetDevice(), &samplerInfo, nullptr, &m_Sampler);
}
