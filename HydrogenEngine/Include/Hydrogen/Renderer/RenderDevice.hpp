#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include "Hydrogen/Renderer/RenderInstance.hpp"

#include <optional>

namespace Hydrogen
{
	class RenderDevice
	{
	public:
		RenderDevice(const RenderDeviceDescriptor& deviceDesc, const std::shared_ptr<Viewport>& viewport);
		~RenderDevice();

		void WaitForIdle() const;

		VkPhysicalDevice GetVulkanPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetVulkanDevice() const { return m_Device; }

		const RenderDeviceDescriptor& GetDescriptor() const { return m_Descriptor; }

		uint32_t GetGraphicsFamilyIndex() const { return m_QueueFamilyIndices.GraphicsFamily.value(); }
		uint32_t GetPresentFamilyIndex() const { return m_QueueFamilyIndices.PresentFamily.value(); }

		VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetPresentQueue() const { return m_PresentQueue; }

		VmaAllocator GetAllocator() const { return m_Allocator; }

		static bool CheckDeviceSuitability(VkPhysicalDevice device, const std::shared_ptr<Viewport>& viewport);

	private:
		struct QueueFamilyIndices
		{
			std::optional<uint32_t> GraphicsFamily;
			std::optional<uint32_t> PresentFamily;

			bool IsComplete() const
			{
				return GraphicsFamily.has_value() && PresentFamily.has_value();
			}
		};

		static bool CheckDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& extensions);
		static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, const std::shared_ptr<Viewport>& viewport);
		static const std::vector<const char*> GetRequiredDeviceExtensions();
		void CreateLogicalDevice();

		VkPhysicalDevice m_PhysicalDevice;
		VkDevice m_Device;
		RenderDeviceDescriptor m_Descriptor;

		QueueFamilyIndices m_QueueFamilyIndices;

		VkQueue m_GraphicsQueue;
		VkQueue m_PresentQueue;

		VmaAllocator m_Allocator = VK_NULL_HANDLE;
	};
}
