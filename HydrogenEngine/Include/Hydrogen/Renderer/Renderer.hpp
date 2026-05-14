#pragma once

#include "RenderContext.hpp"
#include "CommandBuffer.hpp"
#include "RenderGraph.hpp"
#include "DebugGUI.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Camera.hpp"

#define MAX_TEXTURES 128
#define MAX_LIGHTS 16

using PipelineKey = std::pair<std::string, std::string>;

namespace std
{
	template<>
	struct hash<PipelineKey>
	{
		size_t operator()(const PipelineKey& k) const noexcept
		{
			size_t h1 = hash<string>{}(k.first);
			size_t h2 = hash<string>{}(k.second);
			return h1 ^ (h2 << 1);
		}
	};
}

namespace Hydrogen
{
	class Renderer
	{
	public:
		Renderer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Viewport>& viewport);
		Renderer(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height);
		~Renderer();

		struct DebugVertex
		{
			glm::vec3 position;
			glm::vec4 color;
		};

		void Render(const std::shared_ptr<Scene>& scene, CameraComponent& cameraComponent, glm::vec3 cameraPos);

		void Resize(uint32_t width, uint32_t height);
		const std::shared_ptr<Texture>& GetSampledTexture() { return m_SampledTexture; }

		uint32_t GetWidth() const { return m_RenderGraph->GetWidth(); }
		uint32_t GetHeight() const { return m_RenderGraph->GetHeight(); }

	private:
		void InitComponents(const std::shared_ptr<RenderContext>& renderContext, uint32_t width, uint32_t height);

		void BeginFrame(CameraComponent& cameraComponent, glm::vec3 cameraPos);

		void RenderFrame(const std::shared_ptr<RenderGraph>& target);
		void RenderShadowPass(const glm::mat4& lightTransform, const glm::mat4& lightSpaceMatrix, const std::shared_ptr<RenderGraph>& lightRenderGraph);

		void SubmitMesh(const MeshRendererComponent& meshRenderer, const glm::mat4& transform);
		void SubmitLight(const LightComponent& light, const glm::mat4& transform);

		void RenderPostProcessing();

		const std::shared_ptr<Pipeline>& GetOrCreatePipeline(const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader);

		std::shared_ptr<RenderContext> m_RenderContext;

		std::shared_ptr<Texture> m_DefaultTexture;

		std::shared_ptr<CommandBuffer> m_CommandBuffer;
		std::shared_ptr<RenderGraph> m_RenderGraph;
		std::unordered_map<PipelineKey, std::shared_ptr<Pipeline>> m_Pipelines;

		std::shared_ptr<RenderGraph> m_PostProcessingRenderGraph;
		std::shared_ptr<Pipeline> m_PostProcessingPipeline;
		std::shared_ptr<VertexBuffer> m_FullscreenVertexBuffer;
		std::shared_ptr<IndexBuffer> m_FullscreenIndexBuffer;

		std::array<std::shared_ptr<RenderGraph>, MAX_LIGHTS> m_ShadowRenderGraphs;
		std::shared_ptr<Pipeline> m_ShadowPipeline;

		std::shared_ptr<RenderGraph> m_BlurRenderGraphs[2];
		std::shared_ptr<Pipeline> m_BlurPipeline;

		std::shared_ptr<Texture> m_SceneColor;
		std::shared_ptr<Texture> m_SceneBright;
		std::shared_ptr<Texture> m_SampledTexture;

		struct UniformBuffer
		{
			alignas(16) glm::mat4 View;
			alignas(16) glm::mat4 Proj;
			alignas(16) glm::vec3 ViewPos;
			float padding;
		};

		struct ShadowUniformBuffer
		{
			alignas(16) glm::mat4 LightSpaceMatrix;
		};

		struct SceneLightsBuffer
		{
			alignas(16) uint32_t lightCount;
			uint32_t padding[3];
		};

		struct GPULight
		{
			glm::vec4 Position;
			glm::vec4 Color;
			glm::vec4 Direction;
			glm::mat4 LightSpaceMatrix;
		};

		struct PushConstants
		{
			alignas(16) glm::mat4 Model;
			alignas(16) glm::vec4 Color;
			uint32_t TextureIndex;
			uint32_t _Padding[3];
		};

		struct ShadowPushConstants
		{
			alignas(16) glm::mat4 Model;
		};

		struct RenderObject
		{
			std::shared_ptr<VertexBuffer> VertexBuf;
			std::shared_ptr<IndexBuffer> IndexBuf;
			std::shared_ptr<Pipeline> Shader;
			glm::mat4 Transform;
			glm::vec4 Color;
			uint32_t TextureIndex;
		};

		struct BlurPushConstants
		{
			int horizontal;
		};

		struct
		{
			UniformBuffer CameraInfo;
			GPULight Lights[MAX_LIGHTS];
			uint32_t NumLights;

			std::vector<std::shared_ptr<Texture>> Textures;
			std::vector<std::shared_ptr<Texture>> ShadowMaps;
			std::vector<std::shared_ptr<Pipeline>> Pipelines;
			std::vector<RenderObject> Objects;
		} m_FrameInfo;
	};

	class DebugGUIRenderer
	{
	public:
		DebugGUIRenderer(const std::shared_ptr<RenderContext>& renderContext, const std::shared_ptr<Viewport>& viewport);
		~DebugGUIRenderer();

		void Render();
		void Resize(uint32_t width, uint32_t height);

		const std::shared_ptr<DebugGUI>& GetDebugGUI() const { return m_DebugGUI; }

	private:
		std::shared_ptr<RenderContext> m_RenderContext;
		std::shared_ptr<CommandBuffer> m_CommandBuffer;
		std::shared_ptr<RenderGraph> m_RenderGraph;
		std::shared_ptr<DebugGUI> m_DebugGUI;
	};
}
