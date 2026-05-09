#pragma once

#include <memory>
#include <vector>
#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Renderer/RenderTarget.hpp"
#include "Hydrogen/Event.hpp"

namespace Hydrogen
{
	class Pipeline;
	class VertexBuffer;
	class DynamicVertexBuffer;
	class IndexBuffer;

	class CommandBuffer
	{
	public:
		virtual ~CommandBuffer() = default;

		virtual void BeginFrame(const std::shared_ptr<RenderTarget>& renderTarget) = 0;
		virtual void EndFrame() = 0;

		virtual void StartRecording(const std::shared_ptr<RenderTarget>& renderTarget) = 0;
		virtual void EndRecording() = 0;

		virtual void BindPipeline(const std::shared_ptr<Pipeline>& pipeline) = 0;
		virtual void BindVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) = 0;
		virtual void BindDynamicVertexBuffer(const std::shared_ptr<DynamicVertexBuffer>& vertexBuffer) = 0;
		virtual void BindIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) = 0;

		virtual void SetViewport(const std::shared_ptr<RenderTarget>& renderTarget) = 0;
		virtual void SetScissor(const std::shared_ptr<RenderTarget>& renderTarget) = 0;

		virtual void UploadPushConstants(const std::shared_ptr<Pipeline>& pipeline, uint32_t index, void* data) = 0;

		virtual void Draw(size_t numVertices) = 0;
		virtual void DrawIndexed(const std::shared_ptr<IndexBuffer>& indexBuffer) = 0;

		virtual bool IsFrameFinished() const = 0;
		virtual Event<>& GetFrameFinishedEvent() = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<CommandBuffer>& commandBuffer)
		{
			static_assert(std::is_base_of_v<CommandBuffer, T>);
			return std::dynamic_pointer_cast<T>(commandBuffer);
		}

		static std::shared_ptr<CommandBuffer> Create(const std::shared_ptr<RenderContext>& renderContext);
	};
}
