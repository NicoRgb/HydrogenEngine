#ifdef HY_SYSTEM_MACOS

#include "Hydrogen/Platform/MacOS/MacOSViewport.hpp"
#include "Hydrogen/Platform/Vulkan/VulkanRenderContext.hpp"
#include "Hydrogen/Core.hpp"

#include "backends/imgui_impl_glfw.h"

using namespace Hydrogen;

class MacOSBackend
{
public:
	static void Init()
	{
		if (m_Initialized)
		{
			return;
		}

		HY_ASSERT(glfwInit(), "Failed to init GLFW");
		m_Initialized = true;
	}

	static void Terminate()
	{
		glfwTerminate();
	}

private:
	static bool m_Initialized;
};

bool MacOSBackend::m_Initialized = false;

MacOSViewport::MacOSViewport(std::string name, int width, int height, int x, int y)
	: m_Width(width), m_Height(height)
{
	MacOSBackend::Init();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_Window = glfwCreateWindow(m_Width, m_Height, name.c_str(), nullptr, nullptr);
	HY_ASSERT(m_Window, "Failed to create GLFW window");
}

MacOSViewport::~MacOSViewport()
{
	glfwDestroyWindow(m_Window);
}

void MacOSViewport::Open()
{
}

void MacOSViewport::Close()
{
}

const std::vector<const char*> MacOSViewport::GetVulkanExtensions() const
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	std::vector<const char*> glfwInstanceExtensions(glfwExtensionCount+2);
	glfwInstanceExtensions[0] = "VK_KHR_portability_enumeration";
	glfwInstanceExtensions[1] = "VK_KHR_get_physical_device_properties2";

	for (uint32_t i = 0; i < glfwExtensionCount; i++)
	{
		glfwInstanceExtensions[i+2] = glfwExtensions[i];
	}

	return glfwInstanceExtensions;
}

void* MacOSViewport::CreateVulkanSurface(const RenderContext* renderContext) const
{
	VkSurfaceKHR surface;
	const VulkanRenderContext* vkContext = dynamic_cast<const VulkanRenderContext*>(renderContext);

	HY_ASSERT(glfwCreateWindowSurface(vkContext->GetInstance(), m_Window, nullptr, &surface) == VK_SUCCESS, "Failed to create vulkan window surface");

	return (void*)surface;
}

void MacOSViewport::ImGuiNewFrame() const
{
	ImGui_ImplGlfw_NewFrame();
}

void MacOSViewport::InitImGui() const
{
	ImGui_ImplGlfw_InitForVulkan(m_Window, true);
}

void MacOSViewport::PumpMessages()
{
	glfwPollEvents();
}

#endif
