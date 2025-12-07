#pragma once

#include "RenderContext.hpp"
#include "Framebuffer.hpp"
#include "RenderAPI.hpp"
#include "CommandQueue.hpp"
#include "DebugGUI.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Camera.hpp"

namespace Hydrogen
{
	static glm::vec4 UnpackRP3DColor(uint32_t color)
	{
		float r = ((color >> 24) & 0xFF) / 255.0f;
		float g = ((color >> 16) & 0xFF) / 255.0f;
		float b = ((color >> 8) & 0xFF) / 255.0f;
		float a = (color & 0xFF) / 255.0f;

		return { r, g, b, a };
	}

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

		std::shared_ptr<Pipeline> CreatePipeline(const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader);
		void CreateDebugPipelines(const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader);

		void BeginFrame(const std::shared_ptr<Framebuffer>& framebuffer, const std::shared_ptr<RenderPass>& renderPass, CameraComponent& cameraComponent);
		void EndFrame();
		void Draw(const MeshRendererComponent& meshRenderer, const std::shared_ptr<Pipeline>& pipeline, const glm::mat4& transform);

		void BeginDebugGuiFrame(const std::shared_ptr<Framebuffer>& framebuffer, const std::shared_ptr<RenderPass>& renderPass);
		void EndDebugGuiFrame();
		void DrawDebugGui(const std::shared_ptr<DebugGUI>& debugGUI);

		void DrawDebugLines(const std::vector<DebugVertex>& vertices);
		void DrawDebugTriangles(const std::vector<DebugVertex>& vertices);

		const std::shared_ptr<RenderContext>& GetContext() { return m_RenderContext; }
		const std::shared_ptr<RenderAPI>& GetAPI() { return m_RenderAPI; }
		const std::shared_ptr<CommandQueue>& GetCommandQueue() { return m_CommandQueue; }

	private:
		std::shared_ptr<RenderContext> m_RenderContext;
		std::shared_ptr<RenderAPI> m_RenderAPI;
		std::shared_ptr<CommandQueue> m_CommandQueue;
		std::shared_ptr<Framebuffer> m_CurrentFramebuffer;
		std::shared_ptr<Texture> m_DefaultTexture;

		std::shared_ptr<DynamicVertexBuffer> m_DebugLinesVertexBuffer;
		std::shared_ptr<Pipeline> m_DebugLinesShader;
		std::shared_ptr<DynamicVertexBuffer> m_DebugTrianglesVertexBuffer;
		std::shared_ptr<Pipeline> m_DebugTrianglesShader;

		struct UniformBuffer
		{
			alignas(16) glm::mat4 View;
			alignas(16) glm::mat4 Proj;
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
