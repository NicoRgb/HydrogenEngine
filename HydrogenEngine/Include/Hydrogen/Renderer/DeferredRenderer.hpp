#pragma once

#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Renderer/CommandBuffer.hpp"
#include "Hydrogen/Renderer/RenderGraph.hpp"
#include "Hydrogen/Renderer/Pipeline.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Camera.hpp"
#include "Hydrogen/ProceduralMesh.hpp"

namespace Hydrogen
{
	struct Gizmo
	{
		std::shared_ptr<Texture> BillboardTexture;
		glm::vec3 Position;
		glm::vec2 Scale;
	};

	enum class ShadowResolution : uint32_t
	{
		Low = 512,
		Medium = 1024,
		High = 2048,
		Ultra = 4096
	};

	enum class AntiAliasingMode : uint8_t
	{
		None = 0,
		FXAA,
		TAA,
		SMAA
	};

	enum class MSAASampleCount : uint8_t
	{
		None = 1,
		X2 = 2,
		X4 = 4,
		X8 = 8
	};

	enum class TonemappingMethod : uint8_t
	{
		None = 0,
		Reinhard,
		ACES,
		Filmic
	};

	struct DisplaySettings {
		bool EnableVSync = true;
		bool EnableHDR = false;
		bool EnableWireframe = false;
	};

	struct ShadowSettings {
		bool EnableShadows = true;
		ShadowResolution Resolution = ShadowResolution::High;
		bool EnableSoftShadows = true;
	};

	struct AntiAliasingSettings {
		AntiAliasingMode Mode = AntiAliasingMode::None;
		MSAASampleCount MSAASamples = MSAASampleCount::None;
		bool EnableAnisotropicFiltering = true;
	};

	struct LightingSettings {
		bool EnableSSGI = false;  // Screen-Space Global Illumination
		bool EnableSSSR = false;  // Screen-Space Stochastic Reflections
		bool EnableSSR = false;   // Screen-Space Reflections (Standard)
		bool EnableSSAO = false;  // Screen-Space Ambient Occlusion
	};

	struct PostProcessSettings {
		bool EnableBloom = false;
		bool EnableMotionBlur = false;
		bool EnableDepthOfField = false;

		bool EnableToneMapping = true;
		TonemappingMethod Tonemapper = TonemappingMethod::ACES;
	};

	struct CullingSettings {
		bool EnableFrustumCulling = true;
		bool EnableOcclusionCulling = false;
	};

	struct DebugSettings {
		bool EnableDebugOverlays = false;
		bool EnableEntityIDPicking = false;
		bool EnableGizmoRendering = true;
	};

	struct RenderSettings
	{
		DisplaySettings Display;
		ShadowSettings Shadows;
		AntiAliasingSettings AA;
		LightingSettings Lighting;
		PostProcessSettings PostProcess;
		CullingSettings Culling;
		DebugSettings Debug;
	};

	enum RenderDirtyFlags : uint32_t
	{
		Dirty_Clean = 0,
		Dirty_RenderPass = 1 << 0, // Requires recreating RenderPasses/Framebuffers (MSAA changes)
		Dirty_PipelineCache = 1 << 1, // Requires rebuilding pipelines (Wireframe, Shaders)
		Dirty_Swapchain = 1 << 2, // Requires recreating swapchain (VSync, HDR)
		Dirty_Buffers = 1 << 3  // Requires resizing/reallocating textures (Shadow maps)
	};

	class RenderSettingsManager
	{
	public:
		RenderSettingsManager() = default;

		const RenderSettings& Get() const { return m_CurrentSettings; }

		void SetSettings(const RenderSettings& newSettings)
		{
			uint32_t flags = Dirty_Clean;

			if (newSettings.Display.EnableVSync != m_CurrentSettings.Display.EnableVSync ||
				newSettings.Display.EnableHDR != m_CurrentSettings.Display.EnableHDR)
			{
				flags |= Dirty_Swapchain;
			}

			if (newSettings.AA.MSAASamples != m_CurrentSettings.AA.MSAASamples)
			{
				flags |= Dirty_RenderPass | Dirty_PipelineCache;
			}

			if (newSettings.Display.EnableWireframe != m_CurrentSettings.Display.EnableWireframe ||
				newSettings.AA.Mode != m_CurrentSettings.AA.Mode)
			{
				flags |= Dirty_PipelineCache;
			}

			if (newSettings.Shadows.Resolution != m_CurrentSettings.Shadows.Resolution ||
				newSettings.Shadows.EnableShadows != m_CurrentSettings.Shadows.EnableShadows)
			{
				flags |= Dirty_Buffers;
			}

			m_CurrentSettings = newSettings;
			m_DirtyFlags |= flags;
		}

		bool IsDirty() const { return m_DirtyFlags != Dirty_Clean; }
		uint32_t GetDirtyFlags() const { return m_DirtyFlags; }

		void ClearDirtyFlags() { m_DirtyFlags = Dirty_Clean; }

	private:
		RenderSettings m_CurrentSettings;
		uint32_t m_DirtyFlags = Dirty_Clean;
	};

	/*
	optimization ideas:
	- Ping Pong Bloom Passes: Dont create a pass for every pingpong
	- Read/Write Attribute for RenderGraph resources
	- Overlap GPU Texture memory if lifetimes dont overlap
	- look for duplicate shaders
	*/

	class DeferredRenderer
	{
	public:
		DeferredRenderer(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height);
		~DeferredRenderer();

		void Resize(uint32_t width, uint32_t height);
		void Render(const std::shared_ptr<Scene>& scene, CameraComponent& cameraComponent, glm::vec3 cameraPos, const std::vector<Gizmo>& gizmos = {}, const std::shared_ptr<CubeMap>& skybox = nullptr);

		uint32_t ReadEntityIDFromGPU(uint32_t x, uint32_t y);

		uint32_t GetWidth() const { return m_FrameGraph->GetWidth(); }
		uint32_t GetHeight() const { return m_FrameGraph->GetHeight(); }

		std::shared_ptr<Texture> GetFinalSceneTexture() const { return m_FrameGraph->GetTexture("FinalScene"); }
		std::shared_ptr<Texture> GetTexture(std::string name) const { return m_FrameGraph->GetTexture(name); }

		const std::shared_ptr<RenderContext>& GetRenderContext() { return m_RenderContext; }
		const std::shared_ptr<CommandBuffer>& GetCommandBuffer() { return m_CommandBuffer; }

	private:
		void RenderGeometryPass(const std::shared_ptr<FrameGraph>& graph);
		void RenderLightingPass(const std::shared_ptr<FrameGraph>& graph);
		void RenderGizmoPass(const std::shared_ptr<FrameGraph>& graph);
		void RenderBloomPass(const std::shared_ptr<FrameGraph>& graph, std::string passName, std::string sampledTextureName, bool horizontal);
		void RenderCompositePass(const std::shared_ptr<FrameGraph>& graph);

		void UploadMaterialTextures();
		void UploadMaterialTexture(const std::shared_ptr<Texture>& texture, std::unordered_map<Texture*, uint32_t>& textureMap, uint32_t descriptorIndex);
		void RenderPointLights();

		const std::shared_ptr<RenderContext> m_RenderContext;
		std::shared_ptr<CommandBuffer> m_CommandBuffer;

		std::shared_ptr<Texture> m_DefaultTexture;
		std::shared_ptr<VertexBuffer> m_FullscreenVertexBuffer;
		std::shared_ptr<IndexBuffer> m_FullscreenIndexBuffer;
		std::shared_ptr<VertexBuffer> m_SphereVertexBuffer;
		std::shared_ptr<IndexBuffer> m_SphereIndexBuffer;
		std::shared_ptr<VertexBuffer> m_SkyboxVertexBuffer;

		std::shared_ptr<FrameGraph> m_FrameGraph;

		// per frame info
		std::shared_ptr<Scene> m_Scene;
		CameraComponent m_CameraComponent;
		glm::vec3 m_CameraPos;
		std::vector<Gizmo> m_Gizmos;
		std::shared_ptr<CubeMap> m_Skybox;

		std::unordered_map<Texture*, uint32_t> m_AlbedoTextures;
		std::unordered_map<Texture*, uint32_t> m_NormalTextures;
		std::unordered_map<Texture*, uint32_t> m_ORMTextures;
		std::unordered_map<Texture*, uint32_t> m_EmissiveTextures;

		struct CameraInfoUniformBuffer
		{
			glm::mat4 ViewProj;
			glm::vec3 CameraPos;
			float _Padding;
		};

		struct CameraInfoUniformBufferSplit
		{
			glm::mat4 View;
			glm::mat4 Proj;
			glm::vec3 CameraPos;
			float _Padding;
		};

		struct GeometryPassPushConstants
		{
			glm::mat4 Model;

			// MAX_TEXTURES + 1 = no texture
			uint32_t AlbedoIndex;
			uint32_t NormalIndex;
			uint32_t ORMIndex;
			uint32_t EmissiveIndex;

			glm::vec4 Tint;

			float Roughness;
			float Metallic;
			uint32_t ObjectID;
			float Padding0;
			
			glm::vec4 Emissive;
		};

		struct LightingPassPushConstants
		{
			alignas(16) glm::mat4 Model;
			alignas(16) glm::vec3 Color;
			float Intensity;
			alignas(16) glm::vec3 Position;
			float Radius;
		};

		struct DirectionalLightsBuffer
		{
			alignas(16) uint32_t LightCount;
			uint32_t padding[3];
		};

		struct DirectionalLight
		{
			glm::vec3 Color;
			float Intensity;
			glm::vec3 Direction;
			float Padding;
		};

		struct BillboardPushConstants
		{
			glm::vec3 Position;
			int TextureIndex;
			glm::vec2 Scale;
			float Padding0;
			float Padding1;
		};

		struct BlurPushConstants
		{
			int horizontal;
		};

		struct PostProcessingPushConstants
		{
			glm::mat4 ViewProj;
			glm::vec3 ViewPos;
			float Padding;
		};
	};
}
