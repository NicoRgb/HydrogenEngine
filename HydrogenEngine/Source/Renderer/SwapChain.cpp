#include "Hydrogen/Renderer/SwapChain.hpp"
#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Core.hpp"

using namespace Hydrogen;

SwapChain::SwapChain(RenderDevice* device, VkSurfaceKHR surface, const SwapChainSpec& spec)
	: m_Device(device), m_Surface(surface), m_Spec(spec)
{
	QueryCapabilities();
	ChooseCapabilities();
	CreateSwapChain();
	RetrieveImages();
	CreateImageViews();
}

SwapChain::~SwapChain()
{
	for (auto imageView : m_ImageViews)
	{
		vkDestroyImageView(m_Device->GetVulkanDevice(), imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_Device->GetVulkanDevice(), m_SwapChain, nullptr);
}

RgTextureHandle SwapChain::AcquireNextImage(RenderGraph* renderGraph, VkSemaphore semaphore)
{
	VkResult result = vkAcquireNextImageKHR(m_Device->GetVulkanDevice(), m_SwapChain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &m_CurrentImageIndex);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("vkAcquireNextImageKHR return error code {}", (uint16_t)result);
	}

	RgTextureDesc desc;
	desc.Format = GetImageFormat();
	desc.Width = m_Extent.width;
	desc.Height = m_Extent.height;

	return renderGraph->ImportTexture(m_Images[m_CurrentImageIndex], m_ImageViews[m_CurrentImageIndex], desc);
}

void SwapChain::QueryCapabilities()
{
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Device->GetVulkanPhysicalDevice(), m_Surface, &m_Capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_Device->GetVulkanPhysicalDevice(), m_Surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		m_Formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(m_Device->GetVulkanPhysicalDevice(), m_Surface, &formatCount, m_Formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_Device->GetVulkanPhysicalDevice(), m_Surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		m_PresentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(m_Device->GetVulkanPhysicalDevice(), m_Surface, &presentModeCount, m_PresentModes.data());
	}

	m_Extent = m_Capabilities.currentExtent;
}

void SwapChain::ChooseCapabilities()
{
	VkFormat targetVkFormat = VK_FORMAT_B8G8R8A8_SRGB;
	VkColorSpaceKHR targetColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

	if (m_Spec.ColorPreference == ColorFormat::RGBA8_SRGB)
	{
		targetVkFormat = VK_FORMAT_R8G8B8A8_SRGB;
	}
	else if (m_Spec.ColorPreference == ColorFormat::HDR10)
	{
		targetVkFormat = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
		targetColorSpace = VK_COLOR_SPACE_HDR10_ST2084_EXT;
	}

	std::optional<VkSurfaceFormatKHR> fallbackFormat;

	bool formatFound = false;
	for (const auto& available : m_Formats)
	{
		if (available.format == targetVkFormat && available.colorSpace == targetColorSpace)
		{
			m_Format = available;
			formatFound = true;
			break;
		}
		else if (available.format == VK_FORMAT_B8G8R8A8_SRGB && available.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			fallbackFormat = available;
		}
	}

	if (!formatFound)
	{
		if (m_Formats.empty())
		{
			HY_ASSERT(false, "No surface formats are available at all.");
		}
		HY_ENGINE_WARN("Requested surface format not supported by hardware. Falling back to available format.");
		m_Format = m_Formats[0];
		if (fallbackFormat.has_value())
		{
			m_Format = fallbackFormat.value();
		}
	}

	VkPresentModeKHR targetPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	if (m_Spec.VsyncPreference == PresentMode::Immediate)
	{
		targetPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
	else if (m_Spec.VsyncPreference == PresentMode::Mailbox)
	{
		targetPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	}

	bool modeFound = false;
	for (const auto& available : m_PresentModes)
	{
		if (available == targetPresentMode)
		{
			m_PresentMode = available;
			modeFound = true;
			break;
		}
	}

	if (!modeFound)
	{
		HY_ENGINE_WARN("Requested present mode not supported. Falling back to VK_PRESENT_MODE_FIFO_KHR (Standard VSync).");
		m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
	}

	m_ImageFormat = m_Format.format;
}

void SwapChain::CreateSwapChain()
{
	uint32_t imageCount = m_Capabilities.minImageCount + 1;

	if (m_Capabilities.maxImageCount > 0 && imageCount > m_Capabilities.maxImageCount)
	{
		imageCount = m_Capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_Surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = m_ImageFormat;
	createInfo.imageColorSpace = m_Format.colorSpace;
	createInfo.imageExtent = m_Extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (m_Device->GetGraphicsFamilyIndex() != m_Device->GetPresentFamilyIndex())
	{
		uint32_t queueFamilyIndices[2] = { m_Device->GetGraphicsFamilyIndex(), m_Device->GetPresentFamilyIndex() };

		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	createInfo.preTransform = m_Capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = m_PresentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VkResult result = vkCreateSwapchainKHR(m_Device->GetVulkanDevice(), &createInfo, nullptr, &m_SwapChain);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan swap chain... vkCreateSwapchainKHR returned {}", (uint16_t)result);
	}
}

void SwapChain::RetrieveImages()
{
	uint32_t imageCount;
	vkGetSwapchainImagesKHR(m_Device->GetVulkanDevice(), m_SwapChain, &imageCount, nullptr);
	m_Images.resize(imageCount);
	vkGetSwapchainImagesKHR(m_Device->GetVulkanDevice(), m_SwapChain, &imageCount, m_Images.data());
}

void SwapChain::CreateImageViews()
{
	m_ImageViews.resize(m_Images.size());
	for (size_t i = 0; i < m_Images.size(); i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_Images[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_ImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		VkResult result = vkCreateImageView(m_Device->GetVulkanDevice(), &createInfo, nullptr, &m_ImageViews[i]);
		if (result != VK_SUCCESS)
		{
			HY_ENGINE_FATAL("Failed to create Vulkan image view... vkCreateImageView returned {}", (uint16_t)result);
		}
	}
}
