#pragma once

#include <glm/glm.hpp>

#include "Texture.hpp"
#include "Hydrogen/Viewport.hpp"

namespace Hydrogen
{
    enum class AttachmentType
    {
        Color,
        Depth,
        Resolve
    };

    enum class RenderTargetType
    {
        Texture,
        SwapChain
    };

    struct AttachmentSpec
    {
        AttachmentType Type;
        uint32_t SampleCount = 1;
        bool Clear = true;
    };

    struct RenderTargetSpec
    {
        uint32_t Width;
        uint32_t Height;
        std::vector<AttachmentSpec> Attachments;

        RenderTargetType Type;
		std::shared_ptr<Viewport> OutputViewport;

        static RenderTargetSpec ViewportTarget(const std::shared_ptr<Viewport>& viewport, uint32_t msaaSamples = 1);
        static RenderTargetSpec TextureTarget(uint32_t width, uint32_t height, uint32_t msaaSamples = 1);
    };
}
