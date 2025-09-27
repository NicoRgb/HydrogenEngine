#pragma once

#include "Hydrogen/Renderer/Texture.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"

#include "imgui.h"

namespace Hydrogen
{
	class VulkanTexture : public Texture
	{
	public:
		VulkanTexture(const std::shared_ptr<RenderContext>& renderContext);
		~VulkanTexture();

	private:
		const std::shared_ptr<VulkanRenderContext> m_RenderContext;
	};
}
