#pragma once

#include <cstdint>
#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Camera.hpp"

#include <backends/imgui_impl_vulkan.h>

namespace Hydrogen
{
	class Renderer
	{
	public:
		Renderer(const std::shared_ptr<Viewport>& viewport, RenderDevice* device, SwapChain* swapChain);
		Renderer(RenderDevice* device);
		~Renderer();

		void BeginImGuiFrame();

		std::vector<RgTextureView> Render(const std::function<const std::vector<DescriptorBindingValue>(RenderGraph* graph)>& setupPasses, bool present);

		void UpdateSwapChain(SwapChain* swapChain);
		void ClearCache();

		VkSampler GetImguiSampler() const { return m_ImguiSampler; }
		VkSemaphore GetImageAvailableSemaphore() const { return m_ImageAvailableSemaphore; }

		RenderGraph* GetRenderGraph() { return m_RenderGraph.get(); }

	private:
		void CreateCommandBuffer();
		void CreateSyncObjects();
		void InitImGui();

		std::shared_ptr<Viewport> m_Viewport;
		RenderDevice* m_Device;
		SwapChain* m_SwapChain;

		VkCommandBuffer m_CommandBuffer;

		VkSemaphore m_ImageAvailableSemaphore;
		VkSemaphore m_PresentFinishedSemaphore;
		VkFence m_WaitFence;

		std::unique_ptr<RenderGraph> m_RenderGraph;

		VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;

		VkSampler m_ImguiSampler;
	};

	struct DisplaySettings
	{
		uint64_t Width = 1920;
		uint64_t Height = 1080;
	};

	struct RenderSettings
	{
		DisplaySettings Display;
	};

	class DefaultRenderer
	{
	public:
		static RgTextureView DefaultRenderFunc(Renderer* renderer, RenderSettings settings, const CameraComponent& camera, glm::vec3 cameraPos, Scene* scene);
	};

	class ImGuiTextureCache
	{
	public:
		struct CacheKey
		{
			VkImageView ImageView;
			VkSampler Sampler;

			bool operator==(const CacheKey& other) const
			{
				return ImageView == other.ImageView && Sampler == other.Sampler;
			}
		};

		struct CacheValue
		{
			VkDescriptorSet DescriptorSet;
		};

		struct HashKey
		{
			std::size_t operator()(const CacheKey& key) const
			{
				std::size_t h1 = std::hash<void*>()(reinterpret_cast<void*>(key.ImageView));
				std::size_t h2 = std::hash<void*>()(reinterpret_cast<void*>(key.Sampler));
				return h1 ^ (h2 << 1);
			}
		};

		ImGuiTextureCache() = default;

		~ImGuiTextureCache()
		{
			for (auto& [key, value] : m_Cache)
			{
				ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)value.DescriptorSet);
			}
			m_Cache.clear();
		}

		ImTextureID GetTextureID(VkImageView view, VkSampler sampler)
		{
			if (view == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
			{
				return (ImTextureID)VK_NULL_HANDLE;
			}

			CacheKey key{ view, sampler };
			auto it = m_Cache.find(key);

			if (it != m_Cache.end())
			{
				return reinterpret_cast<ImTextureID>(it->second.DescriptorSet);
			}

			VkDescriptorSet descSet = ImGui_ImplVulkan_AddTexture(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

			m_Cache[key] = CacheValue{ descSet };
			return reinterpret_cast<ImTextureID>(descSet);
		}

		void Clear()
		{
			for (auto& [key, value] : m_Cache)
			{
				ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)value.DescriptorSet);
			}
			m_Cache.clear();
		}

	private:
		std::unordered_map<CacheKey, CacheValue, HashKey> m_Cache;
	};
}
