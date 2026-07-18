#pragma once

#include <vulkan/vulkan.h>
#include "Hydrogen/Viewport.hpp"

namespace Hydrogen
{
	enum class RenderDeviceType
	{
		DiscreteGPU,
		IntegratedGPU,
		VirtualGPU,
		CPU,
		Other
	};

	struct RenderDeviceDescriptor
	{
		std::string Name;
		RenderDeviceType Type;
		uint64_t VramBytes;

		size_t ID;
	};

	struct RenderInstanceCreateInfo
	{
		const char* ApplicationName = "Hydrogen Engine";
		uint32_t ApplicationVersion = VK_MAKE_VERSION(1, 0, 0);
		const char* EngineName = "Hydrogen Engine";
		uint32_t EngineVersion = VK_MAKE_VERSION(1, 0, 0);
		uint32_t ApiVersion = VK_API_VERSION_1_2;
	};

	class RenderInstance
	{
	public:
		RenderInstance(const RenderInstanceCreateInfo& info, const std::shared_ptr<Viewport>& viewport);
		~RenderInstance();

		static RenderInstance* Get() { return s_Instance; }

		uint32_t GetVulkanApiVersion() const { return VK_API_VERSION_1_2; }

		const std::vector<RenderDeviceDescriptor> GetRenderDevices() const;
		RenderDeviceDescriptor ChooseRenderDevice(const std::shared_ptr<Viewport>& viewport, const std::function<uint64_t(const RenderDeviceDescriptor&, const std::shared_ptr<Viewport>&)>& scoreFunction = &RenderInstance::DefaultScoreDevice) const;

		VkInstance GetVulkanInstance() const { return m_Instance; }
		VkPhysicalDevice GetVulkanPhysicalDevice(const RenderDeviceDescriptor& deviceDesc) const;

	private:
		void CheckValidationLayerSupport(const std::vector<const char*>& layers);
		void CheckExtensionSupport(const std::vector<const char*>& requiredExtensions);

		void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		void CreateDebugMessenger();

		static uint64_t DefaultScoreDevice(const RenderDeviceDescriptor& deviceDesc, const std::shared_ptr<Viewport>& viewport);
		static RenderDeviceType MapDeviceType(VkPhysicalDeviceType vkType);

		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;

		static RenderInstance* s_Instance;
	};
}
