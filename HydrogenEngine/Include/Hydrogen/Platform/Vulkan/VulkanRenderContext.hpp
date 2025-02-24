#pragma once

#include "Hydrogen/Renderer/RenderContext.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	class VulkanRenderContext : public RenderContext
	{
	public:
		VulkanRenderContext(std::string appName, glm::vec2 appVersion);
		~VulkanRenderContext();

	private:
		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
	};
}
