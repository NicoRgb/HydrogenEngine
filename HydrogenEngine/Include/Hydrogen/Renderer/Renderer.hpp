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
	class Renderer
	{
	public:
		Renderer(const std::shared_ptr<RenderContext>& renderContext);
		Renderer() = default;
		~Renderer();

		std::shared_ptr<Pipeline> CreatePipeline(const std::shared_ptr<RenderPass>& renderPass, const std::shared_ptr<ShaderAsset>& vertexShader, const std::shared_ptr<ShaderAsset>& fragmentShader);

		void BeginFrame(const std::shared_ptr<Framebuffer>& framebuffer, const std::shared_ptr<RenderPass>& renderPass, CameraComponent& cameraComponent);
		void BeginDebugGuiFrame(const std::shared_ptr<Framebuffer>& framebuffer, const std::shared_ptr<RenderPass>& renderPass);
		void EndFrame();
		void Draw(const MeshRendererComponent& meshRenderer, const std::shared_ptr<Pipeline>& pipeline, const glm::mat4& transform);
		void DrawDebugGui(const std::shared_ptr<DebugGUI>& debugGUI);

		const std::shared_ptr<RenderContext>& GetContext() { return m_RenderContext; }
		const std::shared_ptr<RenderAPI>& GetAPI() { return m_RenderAPI; }
		const std::shared_ptr<CommandQueue>& GetCommandQueue() { return m_CommandQueue; }

	private:
		std::shared_ptr<RenderContext> m_RenderContext;
		std::shared_ptr<RenderAPI> m_RenderAPI;
		std::shared_ptr<CommandQueue> m_CommandQueue;

		std::shared_ptr<Framebuffer> m_CurrentFramebuffer;

		std::shared_ptr<Texture> m_DefaultTexture;

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

		struct
		{
			std::vector<std::shared_ptr<Texture>> Textures;
			std::vector<std::shared_ptr<Pipeline>> Pipelines;

			UniformBuffer _UniformBuffer;
		} m_FrameInfo;
	};
}
