#pragma once

#include <cstdint>
#include "Hydrogen/Renderer/RenderGraph.hpp"

#include <backends/imgui_impl_vulkan.h>

namespace Hydrogen
{
	struct DisplaySettings
	{
		uint64_t Width = 1920;
		uint64_t Height = 1080;
	};

	struct RendererSettings
	{
		DisplaySettings Display;
	};

	enum RendererDirtyFlags
	{
		RendererDirtyFlag_Clean = 0,
	};

	class RendererSettingsManager
	{
	public:
		RendererSettingsManager() : m_DirtyFlags(RendererDirtyFlag_Clean) {}
		RendererSettingsManager(const RendererSettings& settings) : m_Settings(settings), m_DirtyFlags(RendererDirtyFlag_Clean) {}
		~RendererSettingsManager() = default;

		void SetSettings(const RendererSettings& settings) { m_Settings = settings; }
		const RendererSettings& GetSettings() const { return m_Settings; }

		bool IsDirty() const { return m_DirtyFlags != RendererDirtyFlag_Clean; }
		uint16_t GetDirtyFlags() const { return m_DirtyFlags; }

	private:
		RendererSettings m_Settings;
		uint16_t m_DirtyFlags;
	};

	class Renderer
	{
	public:
		Renderer(const std::shared_ptr<Viewport>& viewport, RenderDevice* device, SwapChain* swapChain);
		~Renderer();

		void BeginImGuiFrame();
		void Render();
		
		void ClearCache() {}

	private:
		void CreateCommandBuffer();
		void CreateSyncObjects();
		void InitImGui();

		std::shared_ptr<Viewport> m_Viewport;
		RenderDevice* m_Device;
		SwapChain* m_SwapChain;

		VkCommandPool m_CommandPool;
		VkCommandBuffer m_CommandBuffer;

		VkSemaphore m_ImageAvailableSemaphore;
		VkSemaphore m_PresentFinishedSemaphore;
		VkFence m_WaitFence;

		RendererSettingsManager m_SettingsManager;
		std::unique_ptr<RenderGraph> m_RenderGraph;

		VkDescriptorPool m_ImGuiDescriptorPool;

		VkSampler m_ImguiSampler;
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

    private:
        std::unordered_map<CacheKey, CacheValue, HashKey> m_Cache;
    };
}
