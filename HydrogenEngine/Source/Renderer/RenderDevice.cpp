#include "Hydrogen/Renderer/RenderDevice.hpp"
#include "Hydrogen/Logger.hpp"
#include "Hydrogen/Core.hpp"

#include <set>

using namespace Hydrogen;

RenderDevice::RenderDevice(const RenderDeviceDescriptor& deviceDesc, const std::shared_ptr<Viewport>& viewport)
	: m_Descriptor(deviceDesc)
{
	HY_ENGINE_INFO("Creating render device {} (type: {}, VRAM: {} bytes)", deviceDesc.Name, (uint16_t)deviceDesc.Type, deviceDesc.VramBytes);

	m_PhysicalDevice = RenderInstance::Get()->GetVulkanPhysicalDevice(deviceDesc);
	m_QueueFamilyIndices = FindQueueFamilies(m_PhysicalDevice, viewport);

	HY_ASSERT(m_QueueFamilyIndices.IsComplete(), "Failed to find required queue families for render device {}", deviceDesc.Name);
	HY_ASSERT(CheckDeviceExtensionSupport(m_PhysicalDevice, GetRequiredDeviceExtensions()), "Render device {} does not support required extensions", deviceDesc.Name);

	CreateLogicalDevice();

	vkGetDeviceQueue(m_Device, m_QueueFamilyIndices.GraphicsFamily.value(), 0, &m_GraphicsQueue);
	vkGetDeviceQueue(m_Device, m_QueueFamilyIndices.PresentFamily.value(), 0, &m_PresentQueue);

	VmaVulkanFunctions vma_funcs = {};
	vma_funcs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
	vma_funcs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
	allocatorInfo.physicalDevice = m_PhysicalDevice;
	allocatorInfo.device = m_Device;
	allocatorInfo.pVulkanFunctions = &vma_funcs;
	allocatorInfo.instance = RenderInstance::Get()->GetVulkanInstance();
	allocatorInfo.vulkanApiVersion = RenderInstance::Get()->GetVulkanApiVersion();

	VkResult result = vmaCreateAllocator(&allocatorInfo, &m_Allocator);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan memory allocator... vmaCreateAllocator returned {}", (uint16_t)result);
	}
}

RenderDevice::~RenderDevice()
{
	vmaDestroyAllocator(m_Allocator);
	vkDestroyDevice(m_Device, nullptr);
}

void RenderDevice::WaitForIdle() const
{
	vkDeviceWaitIdle(m_Device);
}

bool RenderDevice::CheckDeviceSuitability(VkPhysicalDevice device, const std::shared_ptr<Viewport>& viewport)
{
	QueueFamilyIndices indices = FindQueueFamilies(device, viewport);
	return indices.IsComplete() && CheckDeviceExtensionSupport(device, GetRequiredDeviceExtensions());
}

bool RenderDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

RenderDevice::QueueFamilyIndices RenderDevice::FindQueueFamilies(VkPhysicalDevice device, const std::shared_ptr<Viewport>& viewport)
{
	QueueFamilyIndices indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	int i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, viewport->GetVulkanSurface(), &presentSupport);

		if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.GraphicsFamily = i;
		}

		if (queueFamily.queueFlags & presentSupport)
		{
			indices.PresentFamily = i;
		}

		i++;
	}

	return indices;
}

const std::vector<const char*> RenderDevice::GetRequiredDeviceExtensions()
{
	return { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
}

void RenderDevice::CreateLogicalDevice()
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { m_QueueFamilyIndices.GraphicsFamily.value(), m_QueueFamilyIndices.PresentFamily.value() };

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

	const std::vector<const char*> deviceExtensions = GetRequiredDeviceExtensions();

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

	createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = deviceExtensions.data();

	VkResult result = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device);
	if (result != VK_SUCCESS)
	{
		HY_ENGINE_FATAL("Failed to create Vulkan device... vkCreateDevice returned {}", (uint16_t)result);
	}
}
