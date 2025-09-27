#pragma once

#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Core.hpp"

namespace Hydrogen
{
    enum class VertexElementType
    {
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4
    };

    struct VertexBufferElement
    {
        VertexElementType type;
    };

    using VertexLayout = std::vector<VertexBufferElement>;


	class VertexBuffer
	{
	public:
		virtual ~VertexBuffer() = default;

        static size_t GetVertexSize(VertexLayout layout);

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<VertexBuffer>& vertexBuffer)
		{
			static_assert(std::is_base_of_v<VertexBuffer, T>);

			return std::dynamic_pointer_cast<T>(vertexBuffer);
		}

		static std::shared_ptr<VertexBuffer> Create(const std::shared_ptr<RenderContext>& renderContext, VertexLayout layout, void* vertexData, size_t numVertices);
	};
}
