#include "Hydrogen/Renderer/VertexBuffer.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanVertexBuffer.hpp"

using namespace Hydrogen;

size_t VertexBuffer::GetVertexSize(VertexLayout layout)
{
	size_t size = 0;
	for (size_t i = 0; i < layout.size(); i++)
	{
		switch (layout[i].type)
		{
		case VertexElementType::Float:
			size += 4;
			break;
		case VertexElementType::Float2:
			size += 8;
			break;
		case VertexElementType::Float3:
			size += 12;
			break;
		case VertexElementType::Float4:
			size += 16;
			break;
		case VertexElementType::Int:
			size += 4;
			break;
		case VertexElementType::Int2:
			size += 8;
			break;
		case VertexElementType::Int3:
			size += 12;
			break;
		case VertexElementType::Int4:
			size += 16;
			break;
		default:
			HY_ASSERT(false, "Invalid Vertex Attribute Type");
		}
	}

	return size;
}

std::shared_ptr<VertexBuffer> VertexBuffer::Create(const std::shared_ptr<RenderContext>& renderContext, VertexLayout layout, void* vertexData, size_t numVertices)
{
	return std::make_shared<VulkanVertexBuffer>(renderContext, layout, vertexData, numVertices);
}

std::shared_ptr<DynamicVertexBuffer> DynamicVertexBuffer::Create(const std::shared_ptr<RenderContext>& renderContext, VertexLayout layout, size_t numVertices)
{
	return std::make_shared<VulkanDynamicVertexBuffer>(renderContext, layout, numVertices);
}
