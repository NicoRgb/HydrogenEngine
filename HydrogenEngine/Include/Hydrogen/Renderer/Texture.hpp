#pragma once

#include <memory>

#include "Hydrogen/Renderer/RenderContext.hpp"
#include "imgui.h"

namespace Hydrogen
{
	enum class TextureFormat
	{
		FormatR8G8B8A8, // VK_FORMAT_R8G8B8A8_UNORM
	};

	class Texture
	{
	public:
		virtual ~Texture() = default;

		virtual ImTextureID GetImGuiImage() = 0;

		virtual size_t GetWidth() const = 0;
		virtual size_t GetHeight() const = 0;

		virtual void Resize(size_t width, size_t height) = 0;

		template<typename T>
		static std::shared_ptr<T> Get(const std::shared_ptr<Texture>& texture)
		{
			static_assert(std::is_base_of_v<Texture, T>);

			return std::dynamic_pointer_cast<T>(texture);
		}

		static std::shared_ptr<Texture> Create(const std::shared_ptr<RenderContext>& renderContext, TextureFormat format, size_t width, size_t height);
	};
}
