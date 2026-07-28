#pragma once

#include <cstdint>
#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Scene/Camera.hpp"

#include <backends/imgui_impl_vulkan.h>

namespace Hydrogen
{
	class Renderer
	{
	public:
		Renderer(const std::shared_ptr<Viewport>& viewport, RenderDevice* device, SwapChain* swapChain, uint32_t maxFIF=3);
		Renderer(RenderDevice* device, uint32_t maxFIF=3);
		~Renderer();

		void BeginImGuiFrame();

		std::vector<RgTextureView> Render(const std::function<const std::vector<DescriptorBindingValue>(RenderGraph* graph)>& setupPasses, bool present);

		void UpdateSwapChain(SwapChain* swapChain);
		void ClearCache();

		VkSampler GetImguiSampler() const { return m_ImguiSampler; }
		VkSemaphore GetImageAvailableSemaphore() const { return m_ImageAvailableSemaphores[m_FrameIndex]; }

		RenderGraph* GetRenderGraph() { return m_RenderGraph.get(); }
		SwapChain* GetSwapChain() { return m_SwapChain; }

	private:
		void CreateCommandBuffer();
		void CreateSyncObjects();
		void InitImGui();

		uint32_t m_MaxFIF;
		uint32_t m_FrameIndex = 0;

		std::shared_ptr<Viewport> m_Viewport;
		RenderDevice* m_Device;
		SwapChain* m_SwapChain;

		std::vector<VkCommandBuffer> m_CommandBuffers;

		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkSemaphore> m_PresentFinishedSemaphores;
		std::vector<VkFence> m_WaitFences;

		std::unique_ptr<RenderGraph> m_RenderGraph;

		VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;
		VkSampler m_ImguiSampler;
	};

	struct DisplaySettings
	{
		uint64_t Width = 1920;
		uint64_t Height = 1080;

		bool RenderToSwapChain = true;
	};

	struct Gizmo
	{
		enum class Type
		{
			Billboard,
			WireframeBox,
			WireframeSphere,
			WireframeCapsule
		};

		Type GizmoType = Type::Billboard;
		std::shared_ptr<TextureAsset> BillboardTexture;
		glm::vec3 Position;
		glm::vec2 Scale;

		glm::vec3 WireframeColor = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::quat Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

		glm::vec3 BoxSize = glm::vec3(1.0f);
		float SphereRadius = 1.0f;
		float CapsuleRadius = 0.5f;
		float CapsuleHeight = 1.0f;
		float CylinderRadius = 0.5f;
		float CylinderHeight = 1.0f;
	};

	struct DebugSettings
	{
		bool WireframeMode = false;
		bool RenderGrid = false;
		std::vector<Gizmo> Gizmos;
	};

	struct PostProcessingSettings
	{
		uint8_t BloomIterations = 3;
		bool BloomEnabled = true;
		bool ToneMapping = true;
	};

	struct RenderingSettings
	{
		std::shared_ptr<CubeMapAsset> Skybox = nullptr;
	};

	struct RenderSettings
	{
		DisplaySettings Display;
		DebugSettings Debug;
		PostProcessingSettings PostProcessing;
		RenderingSettings Rendering;
	};

	struct DirectionalLight
	{
		glm::vec3 Color;
		float Intensity;
		glm::vec3 Direction;
		float Padding;
	};

	struct BillboardInstanceData
	{
		glm::vec3 WorldPosition;
		int32_t TextureIndex;
		glm::vec2 Scale;
		glm::vec2 Padding;
	};

	struct GizmoMesh
	{
		std::unique_ptr<RenderBuffer> VertexBuffer;
		std::unique_ptr<RenderBuffer> IndexBuffer;
		uint32_t IndexCount = 0;
	};

	struct GizmoMeshCache
	{
		GizmoMesh BoxMesh;
		GizmoMesh SphereMesh;
		GizmoMesh CapsuleMesh;

		void Initialize(RenderDevice* device);
		void Cleanup();
	};

	class DefaultRenderer
	{
	public:
		static RgTextureView RenderSceneDeferred(Renderer* renderer, RenderSettings settings, const CameraComponent& camera, glm::vec3 cameraPos, Scene* scene);
		static void RenderImGui(Renderer* renderer, SwapChain* swapChain);

		static void Reset()
		{
			s_GizmoMeshCache.reset();
			s_SphereVertexBuffer.reset();
			s_SphereIndexBuffer.reset();
		}

	private:
		static void UploadMaterialTextures(
			Scene* scene,
			std::vector<const Texture*>& albedoTextures,
			std::vector<const Texture*>& normalTextures,
			std::vector<const Texture*>& ORMTextures,
			std::vector<const Texture*>& emissiveTextures);
		static void UploadBones(Scene* scene, std::vector<glm::mat4>& bones, std::vector<uint32_t>& boneBaseIndices);
		static std::vector<DirectionalLight> GetDirectionalLights(Scene* scene);

		static void CollectGizmoRenderData(const std::vector<Gizmo>& gizmos, std::vector<BillboardInstanceData>& instanceData, std::vector<const Texture*>& textures);

		struct GizmoDrawData
		{
			glm::mat4 ModelMatrix;
			glm::vec3 Color;
			Gizmo::Type Type;
		};

		static std::vector<GizmoDrawData> CollectGizmoDrawData(const std::vector<Gizmo>& gizmos);

		static std::unique_ptr<GizmoMeshCache> s_GizmoMeshCache;

		static std::unique_ptr<RenderBuffer> s_SphereVertexBuffer;
		static std::unique_ptr<RenderBuffer> s_SphereIndexBuffer;
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
