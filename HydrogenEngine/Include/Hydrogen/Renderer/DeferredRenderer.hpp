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

	class DeferredRenderer
	{
	public:
		DeferredRenderer(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height);
		~DeferredRenderer();

		void Resize(uint32_t width, uint32_t height);
		void Render(const std::shared_ptr<Scene>& scene, CameraComponent& cameraComponent, glm::vec3 cameraPos);

		void RenderGizmos(const std::vector<Gizmo>& gizmos, CameraComponent& cameraComponent, glm::vec3 cameraPos);

		uint32_t ReadEntityIDFromGPU(uint32_t x, uint32_t y);

		uint32_t GetWidth() const { return m_GBufferRenderGraph->GetWidth(); }
		uint32_t GetHeight() const { return m_GBufferRenderGraph->GetHeight(); }

		std::shared_ptr<Texture> GetSceneColorTexture() const { return m_LightingRenderGraph->GetColorTexture(0); }
		std::shared_ptr<Texture> GetSceneBrightTexture() const { return m_LightingRenderGraph->GetColorTexture(1); }

		const std::shared_ptr<RenderContext>& GetRenderContext() { return m_RenderContext; }
		const std::shared_ptr<CommandBuffer>& GetCommandBuffer() { return m_CommandBuffer; }

	private:
		void RenderGeometryPass(const std::shared_ptr<Scene>& scene, const CameraComponent& cameraComponent, glm::vec3 cameraPos);
		void RenderLightingPass(const std::shared_ptr<Scene>& scene, const CameraComponent& cameraComponent, glm::vec3 cameraPos);

		void UploadMaterialTextures(const std::shared_ptr<Scene>& scene);
		void UploadMaterialTexture(const std::shared_ptr<Texture>& texture, std::unordered_map<Texture*, uint32_t>& textureMap, uint32_t descriptorIndex);
		void RenderPointLights(const std::shared_ptr<Scene>& scene);

		const std::shared_ptr<RenderContext> m_RenderContext;
		std::shared_ptr<CommandBuffer> m_CommandBuffer;

		std::shared_ptr<Texture> m_DefaultTexture;
		std::shared_ptr<VertexBuffer> m_FullscreenVertexBuffer;
		std::shared_ptr<IndexBuffer> m_FullscreenIndexBuffer;
		std::shared_ptr<VertexBuffer> m_SphereVertexBuffer;
		std::shared_ptr<IndexBuffer> m_SphereIndexBuffer;

		std::shared_ptr<RenderGraph> m_GBufferRenderGraph;
		std::shared_ptr<Pipeline> m_GBufferPipeline;

		std::shared_ptr<RenderGraph> m_LightingRenderGraph;
		std::shared_ptr<Pipeline> m_DirectionalLightsPipeline;
		std::shared_ptr<Pipeline> m_PointLightPipeline;

		std::shared_ptr<RenderGraph> m_GizmoRenderGraph;
		std::shared_ptr<Pipeline> m_BillboardPipeline;
		std::shared_ptr<Pipeline> m_GridPipeline;

		// per frame info
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
	};

	class PostProcessing
	{
	public:
		void PostProcess(const std::shared_ptr<DeferredRenderer>& renderer, uint32_t width, uint32_t height);
		const std::shared_ptr<Texture>& PostProcessOffscreen(const std::shared_ptr<DeferredRenderer>& renderer, uint32_t width, uint32_t height);

		const std::shared_ptr<Texture>& GetFinalImage() { if (!m_PostProcessingOffscreenRenderGraph) return nullptr; return m_PostProcessingOffscreenRenderGraph->GetColorTexture(0); }

	private:
		void InitComponents(const std::shared_ptr<DeferredRenderer>& renderer, uint32_t width, uint32_t height);
		void PostProcess(const std::shared_ptr<DeferredRenderer>& renderer, uint32_t width, uint32_t height, const std::shared_ptr<RenderGraph>& renderGraph);

		void Resize(uint32_t width, uint32_t height);

		std::shared_ptr<RenderGraph> m_PostProcessingRenderGraph;
		std::shared_ptr<RenderGraph> m_PostProcessingOffscreenRenderGraph;

		std::shared_ptr<Pipeline> m_PostProcessingPipeline;
		std::shared_ptr<VertexBuffer> m_FullscreenVertexBuffer;
		std::shared_ptr<IndexBuffer> m_FullscreenIndexBuffer;

		std::shared_ptr<RenderGraph> m_BlurRenderGraphs[2];
		std::shared_ptr<Pipeline> m_BlurPipeline;

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
