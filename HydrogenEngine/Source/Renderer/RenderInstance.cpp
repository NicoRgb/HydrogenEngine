#include "Hydrogen/Renderer/RenderInstance.hpp"
#include "Hydrogen/Renderer/RenderDevice.hpp"
#include "Hydrogen/Logger.hpp"

#include <set>

using namespace Hydrogen;

RenderInstance* RenderInstance::s_Instance = nullptr;

template <typename T>
static bool VectorIsSubset(std::vector<T> A, std::vector<T> B)
{
	std::sort(A.begin(), A.end());
	std::sort(B.begin(), B.end());
	return std::includes(A.begin(), A.end(), B.begin(), B.end());
}

static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugMessangerCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	switch (messageSeverity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		HY_ENGINE_TRACE("Vulkan validation layer: {}", pCallbackData->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		HY_ENGINE_INFO("Vulkan validation layer: {}", pCallbackData->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		HY_ENGINE_WARN("Vulkan validation layer: {}", pCallbackData->pMessage);
		break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		HY_ENGINE_ERROR("Vulkan validation layer: {}", pCallbackData->pMessage);
		break;
	}

	return VK_FALSE;
}

RenderInstance::RenderInstance(const RenderInstanceCreateInfo& info, const std::shared_ptr<Viewport>& viewport)
{
	if (s_Instance)
	{
		HY_ENGINE_ERROR("Attempted to create multiple RenderInstance instances");
		return;
	}

	s_Instance = this;

	// validation layers
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation"
	};

	#ifdef HY_DEBUG
		constexpr bool enableValidationLayers = true;
		CheckValidationLayerSupport(validationLayers);
	#else
		constexpr bool enableValidationLayers = false;
	#endif

	// extensions
	auto extensions = viewport->GetVulkanExtensions();
	if (enableValidationLayers)
	{
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
	CheckExtensionSupport(extensions);

	// app info
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = info.ApplicationName;
	appInfo.applicationVersion = info.ApplicationVersion;
	appInfo.pEngineName = info.EngineName;
	appInfo.engineVersion = info.EngineVersion;
	appInfo.apiVersion = info.ApiVersion;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();
	if (enableValidationLayers)
	{
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		PopulateDebugMessengerCreateInfo(debugCreateInfo);

		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan instance... vkCreateInstance returned {}", (uint16_t)result);
	}

	if (enableValidationLayers)
	{
		CreateDebugMessenger();
	}
}

RenderInstance::~RenderInstance()
{
	if (m_DebugMessenger)
	{
		DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
	}

	vkDestroyInstance(m_Instance, nullptr);
	s_Instance = nullptr;
}

const std::vector<RenderDeviceDescriptor> RenderInstance::GetRenderDevices() const
{
	std::vector<RenderDeviceDescriptor> devices;

	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

	if (deviceCount == 0) 
	{
		return devices;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, physicalDevices.data());

	for (size_t i = 0; i < physicalDevices.size(); i++)
	{
		VkPhysicalDevice device = physicalDevices[i];

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		VkPhysicalDeviceMemoryProperties memoryProperties;
		vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);

		uint64_t totalVram = 0;
		for (uint32_t j = 0; j < memoryProperties.memoryHeapCount; ++j)
		{
			if (memoryProperties.memoryHeaps[j].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
			{
				totalVram += memoryProperties.memoryHeaps[j].size;
			}
		}

		RenderDeviceDescriptor info;
		info.Name = properties.deviceName;
		info.Type = MapDeviceType(properties.deviceType);
		info.VramBytes = totalVram;
		info.ID = i;

		devices.push_back(info);
	}

	return devices;
}

RenderDeviceDescriptor RenderInstance::ChooseRenderDevice(const std::shared_ptr<Viewport>& viewport, const std::function<uint64_t(const RenderDeviceDescriptor&, const std::shared_ptr<Viewport>&)>& scoreFunction) const
{
	RenderDeviceDescriptor bestDevice = {};
	uint64_t highestScore = 0;

	for (const auto& device : GetRenderDevices())
	{
		uint64_t score = scoreFunction(device, viewport);
		if (score > highestScore)
		{
			highestScore = score;
			bestDevice = device;
		}
	}

	return bestDevice;
}

void RenderInstance::CheckValidationLayerSupport(const std::vector<const char*>& layers)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	std::set<std::string> requestedLayers(layers.begin(), layers.end());
	for (const auto& extension : availableLayers)
	{
		requestedLayers.erase(extension.layerName);
	}

	if (!requestedLayers.empty())
	{
		HY_ENGINE_WARN("{} requested Vulkan validation layers are not supported", requestedLayers.size());
	}
}

void RenderInstance::CheckExtensionSupport(const std::vector<const char*>& requiredExtensions)
{
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	std::set<std::string> requestedExtensions(requiredExtensions.begin(), requiredExtensions.end());
	for (const auto& extension : extensions)
	{
		requestedExtensions.erase(extension.extensionName);
	}

	if (!requestedExtensions.empty())
	{
		HY_ENGINE_WARN("{} requested Vulkan extensions are not supported", requestedExtensions.size());
	}
}

void RenderInstance::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = DebugMessangerCallback;
	createInfo.pUserData = nullptr;
}

void RenderInstance::CreateDebugMessenger()
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	PopulateDebugMessengerCreateInfo(createInfo);

	VkResult result = CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan debug messenger... CreateDebugUtilsMessengerEXT returned {}", (uint16_t)result);
	}
}

VkPhysicalDevice RenderInstance::GetVulkanPhysicalDevice(const RenderDeviceDescriptor& deviceDesc) const
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		return VK_NULL_HANDLE;
	}

	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_Instance, &deviceCount, physicalDevices.data());
	return physicalDevices[deviceDesc.ID];
}

uint64_t RenderInstance::DefaultScoreDevice(const RenderDeviceDescriptor& deviceDesc, const std::shared_ptr<Viewport>& viewport)
{
	VkPhysicalDevice device = s_Instance->GetVulkanPhysicalDevice(deviceDesc);
	if (!RenderDevice::CheckDeviceSuitability(device, viewport))
	{
		return 0;
	}

	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	uint64_t score = 0;

	if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
	{
		score += 1000;
	}
	else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
	{
		score += 100;
	}
	else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
	{
		score += 50;
	}

	score += deviceProperties.limits.maxImageDimension2D;

	if (!deviceFeatures.geometryShader)
	{
		return 0;
	}

	uint64_t vramMB = deviceDesc.VramBytes / (1024 * 1024);
	score += vramMB;

	return score;
}

RenderDeviceType RenderInstance::MapDeviceType(VkPhysicalDeviceType vkType)
{
	switch (vkType)
	{
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		return RenderDeviceType::DiscreteGPU;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		return RenderDeviceType::IntegratedGPU;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		return RenderDeviceType::VirtualGPU;
	default:
		return RenderDeviceType::Other;
	}
}
