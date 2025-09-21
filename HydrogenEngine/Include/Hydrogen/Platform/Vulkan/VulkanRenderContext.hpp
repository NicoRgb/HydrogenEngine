#pragma once

#include "Hydrogen/Renderer/RenderContext.hpp"
#include <vulkan/vulkan.h>

namespace Hydrogen
{
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

	class VulkanRenderContext : public RenderContext
	{
	public:
		VulkanRenderContext(std::string appName, glm::vec2 appVersion, const std::shared_ptr<Viewport>& viewport);
		~VulkanRenderContext();

		void CreateSwapChain();

		void OnResize(int width, int height) override;

		const VkInstance& GetInstance() const { return m_Instance; }
		const VkDevice& GetDevice() const { return m_Device; }
		const VkSwapchainKHR& GetSwapChain() const { return m_SwapChain; }
		const VkFormat& GetSwapChainImageFormat() const { return m_SwapChainImageFormat; }
		const VkExtent2D& GetSwapChainExtent() const { return m_SwapChainExtent; }
		const std::vector<VkImageView>& GetSwapChainImageViews() const { return m_SwapChainImageViews; }
		const uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
		const VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		const VkQueue GetPresentQueue() const { return m_PresentQueue; }
		const VkCommandPool GetCommandPool() const { return m_CommandPool; }

	private:
		uint64_t ScorePhysicalDevice(VkPhysicalDevice physicalDevice, const std::vector<const char*>& deviceExtensions);
		bool FindQueueFamilies(VkPhysicalDevice physicalDevice);
		SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice) const;
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
		VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

		std::shared_ptr<Viewport> m_Viewport;

		VkInstance m_Instance;
		VkDebugUtilsMessengerEXT m_DebugMessenger;
		VkSurfaceKHR m_Surface;
		VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
		VkDevice m_Device;
		VkSwapchainKHR m_SwapChain;

		uint32_t m_GraphicsQueueFamily;
		uint32_t m_PresentQueueFamily;
		VkQueue m_GraphicsQueue;
		VkQueue m_PresentQueue;

		std::vector<VkImageView> m_SwapChainImageViews;
		std::vector<VkImage> m_SwapChainImages;
		VkFormat m_SwapChainImageFormat;
		VkExtent2D m_SwapChainExtent;

		VkCommandPool m_CommandPool;
	};
}
