#pragma once

#include "RenderContext.hpp"
#include "RenderTarget.hpp"
#include "CommandBuffer.hpp"
#include "DebugGUI.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Camera.hpp"

namespace Hydrogen
{
	class Renderer
	{
	public:
		Renderer(const std::shared_ptr<RenderContext>& renderContext);
		Renderer() = default;
		~Renderer();

		struct DebugVertex
		{
			glm::vec3 position;
			glm::vec4 color;
		};

		std::shared_ptr<Pipeline> CreatePipeline(const std::shared_ptr<RenderTarget>& renderTarget, 
		                                          const std::shared_ptr<ShaderAsset>& vertexShader, 
		                                          const std::shared_ptr<ShaderAsset>& fragmentShader);
		void CreateDebugPipelines(const std::shared_ptr<RenderTarget>& renderTarget, 
		                          const std::shared_ptr<ShaderAsset>& vertexShader, 
		                          const std::shared_ptr<ShaderAsset>& fragmentShader);

		void BeginFrame(const std::shared_ptr<RenderTarget>& renderTarget, CameraComponent& cameraComponent, glm::vec3 cameraPos);
		void EndFrame();
		void Draw(const MeshRendererComponent& meshRenderer, const std::shared_ptr<Pipeline>& pipeline, const glm::mat4& transform);

		void BeginDebugGuiFrame(const std::shared_ptr<RenderTarget>& renderTarget);
		void EndDebugGuiFrame();
		void DrawDebugGui(const std::shared_ptr<DebugGUI>& debugGUI);

		void DrawDebugLines(const std::vector<DebugVertex>& vertices);
		void DrawDebugTriangles(const std::vector<DebugVertex>& vertices);

		const std::shared_ptr<RenderContext>& GetContext() { return m_RenderContext; }
		const std::shared_ptr<CommandBuffer>& GetCommandBuffer() { return m_CommandBuffer; }

	private:
		std::shared_ptr<RenderContext> m_RenderContext;
		std::shared_ptr<CommandBuffer> m_CommandBuffer;
		std::shared_ptr<RenderTarget> m_CurrentRenderTarget;
		std::shared_ptr<Texture> m_DefaultTexture;

		std::shared_ptr<DynamicVertexBuffer> m_DebugLinesVertexBuffer;
		std::shared_ptr<Pipeline> m_DebugLinesShader;
		std::shared_ptr<DynamicVertexBuffer> m_DebugTrianglesVertexBuffer;
		std::shared_ptr<Pipeline> m_DebugTrianglesShader;

		struct UniformBuffer
		{
			alignas(16) glm::mat4 View;
			alignas(16) glm::mat4 Proj;
			alignas(16) glm::vec3 ViewPos;
			float padding;
		};

		struct SceneLightsBuffer
		{
			alignas(16) uint32_t lightCount;
			uint32_t padding[3];
		};

		struct GPULight
		{
			alignas(16) glm::vec4 position;
			alignas(16) glm::vec4 color;
		};

		struct PushConstants
		{
			alignas(16) glm::mat4 Model;
			alignas(16) uint32_t TextureIndex;
		};

		struct RenderObject
		{
			std::shared_ptr<VertexBuffer> VertexBuf;
			std::shared_ptr<IndexBuffer> IndexBuf;
			std::shared_ptr<Pipeline> Shader;
			glm::mat4 Transform;
			uint32_t TextureIndex;
		};

		struct
		{
			UniformBuffer _UniformBuffer;

			std::vector<std::shared_ptr<Texture>> Textures;
			std::vector<std::shared_ptr<Pipeline>> Pipelines;
			size_t NumDebugLineVertices;
			size_t NumDebugTriangleVertices;

			std::vector<RenderObject> Objects;
		} m_FrameInfo;
	};
}
