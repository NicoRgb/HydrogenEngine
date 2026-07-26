#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Renderer/RenderBuffer.hpp"
#include "Hydrogen/Core.hpp"
#include <backends/imgui_impl_vulkan.h>

using namespace Hydrogen;

Texture::Texture(RenderDevice* device, const TextureDescription& desc)
	: m_Device(device), m_Desc(desc)
{
	ExtractVulkanFlags();
	CreateImageVMA();
	CreateImageView();
}

Texture::Texture(RenderDevice* device, VkImage image, VkImageView imageView, const TextureDescription& desc)
	: m_Device(device), m_Desc(desc), m_Image(image), m_ImageView(imageView)
{
	ExtractVulkanFlags();
}

Texture::~Texture()
{
	if (m_ImageView != VK_NULL_HANDLE) vkDestroyImageView(m_Device->GetVulkanDevice(), m_ImageView, nullptr);
	vmaDestroyImage(m_Device->GetAllocator(), m_Image, m_Allocation);
}

void Texture::UploadData(uint32_t* data, uint32_t width, uint32_t height)
{
	uint32_t layerCount = (m_Desc.Type == TextureType::CubeMap) ? 6 : 1;
	VkDeviceSize imageSize = width * height * 4 * layerCount;

	RenderBuffer stagingBuffer(m_Device, BufferDescription{ imageSize, BufferType::Staging, true });
	stagingBuffer.UploadData(data, imageSize);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_Device->GetCommandPool();
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(m_Device->GetVulkanDevice(), &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	// Barrier: UNDEFINED -> TRANSFER_DST
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = m_Image;
	barrier.subresourceRange.aspectMask = m_AspectMask;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layerCount;

	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	// Copy Buffer to Image
	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = m_AspectMask;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = layerCount;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.GetBuffer(), m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	// Barrier: TRANSFER_DST -> SHADER_READ
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStage, destinationStage,
		0,
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_Device->GetGraphicsQueue());

	vkFreeCommandBuffers(m_Device->GetVulkanDevice(), m_Device->GetCommandPool(), 1, &commandBuffer);
}

void Texture::ExtractVulkanFlags()
{
	m_VkFormat = VK_FORMAT_R8G8B8A8_SRGB;
	m_AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	switch (m_Desc.Format)
	{
	case TextureFormat::RGBA8_SRGB:    m_VkFormat = VK_FORMAT_R8G8B8A8_SRGB; break;
	case TextureFormat::RGBA16_SFLOAT: m_VkFormat = VK_FORMAT_R16G16B16A16_SFLOAT; break;
	case TextureFormat::BGRA8_SRGB:    m_VkFormat = VK_FORMAT_B8G8R8A8_SRGB; break;
	case TextureFormat::D32_SFLOAT:
		m_VkFormat = VK_FORMAT_D32_SFLOAT;
		m_AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		break;
	}

	m_UsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	if (((uint32_t)m_Desc.UsageFlags & (uint32_t)TextureUsage::SampledImage) == (uint32_t)TextureUsage::SampledImage)
	{
		m_UsageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
	}
	if (((uint32_t)m_Desc.UsageFlags & (uint32_t)TextureUsage::ColorAttachment) == (uint32_t)TextureUsage::ColorAttachment)
	{
		m_UsageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	}
	if (((uint32_t)m_Desc.UsageFlags & (uint32_t)TextureUsage::DepthAttachment) == (uint32_t)TextureUsage::DepthAttachment)
	{
		m_UsageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
}

void Texture::CreateImageVMA()
{
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = m_Desc.Width;
	imageInfo.extent.height = m_Desc.Height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = m_VkFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = m_UsageFlags;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (m_Desc.Type == TextureType::CubeMap)
	{
		imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		imageInfo.arrayLayers = 6;
	}

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

	VkResult result = vmaCreateImage(
		m_Device->GetAllocator(),
		&imageInfo,
		&allocInfo,
		&m_Image,
		&m_Allocation,
		nullptr
	);

	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan image... vmaCreateImage returned {}", (uint16_t)result);
	}
}

void Texture::CreateImageView()
{
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_Image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_VkFormat;
	viewInfo.subresourceRange.aspectMask = m_AspectMask;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (m_Desc.Type == TextureType::CubeMap)
	{
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewInfo.subresourceRange.layerCount = 6;
	}

	VkResult result = vkCreateImageView(m_Device->GetVulkanDevice(), &viewInfo, nullptr, &m_ImageView);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan image view... vkCreateImageView returned {}", (uint16_t)result);
	}
}
