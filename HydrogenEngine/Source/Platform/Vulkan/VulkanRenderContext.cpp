#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Core.hpp"

#include <set>

using namespace Hydrogen;

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

static bool CheckExtensionSupport(const char* extension)
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	for (const auto& layerProperties : extensions)
	{
		if (strcmp(extension, layerProperties.extensionName) == 0)
		{
			return true;
		}
	}

	return false;
}

static bool CheckDeviceExtensionsSupport(VkPhysicalDevice physicalDevice, const std::vector<const char*>& deviceExtensions)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

static bool CheckValidationLayerSupport(const char* validationLayer)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const auto& layerProperties : availableLayers)
	{
		if (strcmp(validationLayer, layerProperties.layerName) == 0)
		{
			return true;
		}
	}

	return false;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	switch (messageSeverity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		HY_ENGINE_DEBUG("Vulkan validation layer message:\n{}", pCallbackData->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		HY_ENGINE_INFO("Vulkan validation layer message:\n{}", pCallbackData->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		HY_ENGINE_WARN("Vulkan validation layer message:\n{}", pCallbackData->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		HY_ENGINE_ERROR("Vulkan validation layer message:\n{}", pCallbackData->pMessage);
		break;
	}

	return VK_FALSE;
}

VulkanRenderContext::VulkanRenderContext(std::string appName, glm::vec2 appVersion, const std::shared_ptr<Viewport>& viewport)
	: m_Viewport(viewport)
{
	std::vector<const char*> extensions = viewport->GetVulkanExtensions();
#ifdef HY_DEBUG
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
	for (const char* extension : extensions)
	{
		if (!CheckExtensionSupport(extension))
		{
			HY_ASSERT(false, "Unsupported extension layer");
		}
	}

	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	for (const char* validationLayer : validationLayers)
	{
		if (!CheckValidationLayerSupport(validationLayer))
		{
			HY_ASSERT(false, "Unsupported validation layer");
		}
	}

#ifdef HY_DEBUG
	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo{};
	debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugMessengerCreateInfo.pfnUserCallback = DebugCallback;
#endif

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(appVersion.x, appVersion.y, 0);
	appInfo.pEngineName = "Hydrogen";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = (uint32_t)extensions.size();
	createInfo.ppEnabledExtensionNames = extensions.data();
#ifdef HY_DEBUG
	createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
	createInfo.ppEnabledLayerNames = validationLayers.data();
	createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugMessengerCreateInfo;
#endif

	HY_ASSERT(vkCreateInstance(&createInfo, nullptr, &m_Instance) == VK_SUCCESS, "Failed to create vulkan instance");

#ifdef HY_DEBUG
	HY_ASSERT(CreateDebugUtilsMessengerEXT(m_Instance, &debugMessengerCreateInfo, nullptr, &m_DebugMessenger) == VK_SUCCESS, "Failed to create vulkan debug messenger");
#endif

	m_Surface = (VkSurfaceKHR)viewport->CreateVulkanSurface(this);

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
	HY_ASSERT(deviceCount, "Failed to find GPU with vulkan support");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

	std::multimap<uint64_t, VkPhysicalDevice> candidates;

	for (const auto& device : devices)
	{
		uint64_t score = ScorePhysicalDevice(device, deviceExtensions);
		candidates.insert(std::make_pair(score, device));
	}

	HY_ASSERT(candidates.rbegin()->first, "No suitable GPU was found");
	m_PhysicalDevice = candidates.rbegin()->second;

	{
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { m_GraphicsQueueFamily, m_PresentQueueFamily };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
#ifdef HY_DEBUG
		createInfo.enabledLayerCount = (uint32_t)validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
#endif

		HY_ASSERT(vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) == VK_SUCCESS, "Failed to create vulkan device");
	}

	vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, m_PresentQueueFamily, 0, &m_PresentQueue);

	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

		m_SwapChainImageFormat = surfaceFormat.format;
		m_SwapChainExtent = extent;

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_Surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		uint32_t queueFamilyIndices[] = { m_GraphicsQueueFamily, m_PresentQueueFamily };

		if (m_GraphicsQueueFamily != m_PresentQueueFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0; // Optional
			createInfo.pQueueFamilyIndices = nullptr; // Optional
		}

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		HY_ASSERT(vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain) == VK_SUCCESS, "Failed to create vulkan swapchain");

		vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());
	}

	m_SwapChainImageViews.resize(m_SwapChainImages.size());
	for (size_t i = 0; i < m_SwapChainImages.size(); i++)
	{
		VkImageViewCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		createInfo.image = m_SwapChainImages[i];
		createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		createInfo.format = m_SwapChainImageFormat;
		createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;

		HY_ASSERT(vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapChainImageViews[i]) == VK_SUCCESS, "Failed to create vulkan image view");
	}
}

VulkanRenderContext::~VulkanRenderContext()
{
	for (auto imageView : m_SwapChainImageViews)
	{
		vkDestroyImageView(m_Device, imageView, nullptr);
	}

	vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
	vkDestroyDevice(m_Device, nullptr);
#ifdef HY_DEBUG
	DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
#endif
	vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
	vkDestroyInstance(m_Instance, nullptr);
}

uint64_t VulkanRenderContext::ScorePhysicalDevice(VkPhysicalDevice physicalDevice, const std::vector<const char*>& deviceExtensions)
{
	if (!FindQueueFamilies(physicalDevice) || !CheckDeviceExtensionsSupport(physicalDevice, deviceExtensions))
	{
		return 0;
	}

	bool swapChainAdequate = false;
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice);
	swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();

	if (!swapChainAdequate)
	{
		return 0;
	}

	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	uint64_t score = 0;
	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 10000000;
	}

	uint64_t memoryCount = 0;
	for (uint32_t i = 0; i < deviceMemoryProperties.memoryHeapCount; i++)
	{
		memoryCount += deviceMemoryProperties.memoryHeaps[i].size;
	}

	return score;
}

bool VulkanRenderContext::FindQueueFamilies(VkPhysicalDevice physicalDevice)
{
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	bool foundGraphicsQueueFamily = false;
	bool foundPresentQueueFamily = false;
	for (const auto& queueFamily : queueFamilies)
	{
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, m_Surface, &presentSupport);

		if (presentSupport)
		{
			m_PresentQueueFamily = i;
			foundPresentQueueFamily = true;
		}

		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			m_GraphicsQueueFamily = i;
			foundGraphicsQueueFamily = true;
		}

		if (foundGraphicsQueueFamily && foundPresentQueueFamily)
		{
			break;
		}

		i++;
	}

	return foundGraphicsQueueFamily && foundPresentQueueFamily;
}

SwapChainSupportDetails VulkanRenderContext::QuerySwapChainSupport(VkPhysicalDevice physicalDevice) const
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, m_Surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_Surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_Surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR VulkanRenderContext::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const
{
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR VulkanRenderContext::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const
{
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderContext::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}


	VkExtent2D actualExtent = { static_cast<uint32_t>(m_Viewport->GetWidth()), static_cast<uint32_t>(m_Viewport->GetHeight()) };

	actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
	actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

	return actualExtent;
}
