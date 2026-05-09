#include "Hydrogen/Renderer/RenderTargetSpec.hpp"

using namespace Hydrogen;

RenderTargetSpec RenderTargetSpec::ViewportTarget(const std::shared_ptr<Viewport>& viewport, uint32_t msaaSamples)
{
    RenderTargetSpec spec;
    spec.Width = viewport->GetWidth();
    spec.Height = viewport->GetHeight();
    spec.Attachments = {
        { AttachmentType::Color, msaaSamples, true },
        { AttachmentType::Depth, msaaSamples, true }
    };
    if (msaaSamples > 1)
    {
        spec.Attachments.push_back({ AttachmentType::Resolve, 1, false });
    }

	spec.Type = RenderTargetType::SwapChain;
	spec.OutputViewport = viewport;

    return spec;
}

RenderTargetSpec RenderTargetSpec::TextureTarget(uint32_t width, uint32_t height, uint32_t msaaSamples)
{
    RenderTargetSpec spec;
    spec.Width = width;
    spec.Height = height;
    spec.Attachments = {
        { AttachmentType::Color, msaaSamples, true },
        { AttachmentType::Depth, msaaSamples, true }
    };
    if (msaaSamples > 1)
    {
        spec.Attachments.push_back({ AttachmentType::Resolve, 1, false });
    }

    spec.Type = RenderTargetType::Texture;

    return spec;
}
