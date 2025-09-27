#pragma once

#include "Hydrogen/Renderer/RenderContext.hpp"
#include "Hydrogen/Core.hpp"

namespace Hydrogen
{
    class IndexBuffer
    {
    public:
        virtual ~IndexBuffer() = default;

        template<typename T>
        static std::shared_ptr<T> Get(const std::shared_ptr<IndexBuffer>& indexBuffer)
        {
            static_assert(std::is_base_of_v<IndexBuffer, T>);

            return std::dynamic_pointer_cast<T>(indexBuffer);
        }

        virtual const size_t GetNumIndices() const = 0;

        static std::shared_ptr<IndexBuffer> Create(const std::shared_ptr<RenderContext>& renderContext, const std::vector<uint16_t>& indices);
    };
}
