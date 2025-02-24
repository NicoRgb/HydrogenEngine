#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Core.hpp"

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

static int ScorePhysicalDevice(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceMemoryProperties(device, &deviceMemoryProperties);

	int score = 0;
	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 10000000;
	}
	
	int memoryCount = 0;
	for (int i = 0; i < deviceMemoryProperties.memoryHeapCount; i++)
	{
		memoryCount += deviceMemoryProperties.memoryHeaps[i].size;
	}

	return score;
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

VulkanRenderContext::VulkanRenderContext(std::string appName, glm::vec2 appVersion)
{
	std::vector<const char*> extensions;
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

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
	HY_ASSERT(deviceCount, "Failed to find GPU with vulkan support");

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

	std::multimap<int, VkPhysicalDevice> candidates;

	for (const auto& device : devices)
	{
		int score = ScorePhysicalDevice(device);
		candidates.insert(std::make_pair(score, device));
	}

	HY_ASSERT(candidates.rbegin()->first, "No suitable GPU was found");
	m_PhysicalDevice = candidates.rbegin()->second;
}

VulkanRenderContext::~VulkanRenderContext()
{
#ifdef HY_DEBUG
	DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
#endif
	vkDestroyInstance(m_Instance, nullptr);
}
