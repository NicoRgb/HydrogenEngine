#ifdef HY_SYSTEM_MACOS

#pragma once

#include "Hydrogen/Viewport.hpp"

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Hydrogen
{
	class MacOSViewport : public Viewport
	{
	public:
		MacOSViewport(std::string name, int width, int height, int x, int y);
		~MacOSViewport();

		void Open() override;
		void Close() override;

		int GetWidth() const override { return m_Width; }
		int GetHeight() const override { return m_Height; }
		int IsOpen() const override { return !glfwWindowShouldClose(m_Window); }

		const std::vector<const char*> GetVulkanExtensions() const override;
		void* CreateVulkanSurface(const class RenderContext* renderContext) const override;
		void ImGuiNewFrame() const override;
		void InitImGui() const override;

		static void PumpMessages();

	private:
		int m_Width, m_Height;

		GLFWwindow* m_Window;
	};
}

#endif
