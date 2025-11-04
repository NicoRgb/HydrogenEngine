#pragma once

#include <string>
#include <glm/glm.hpp>

#include "Hydrogen/Viewport.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Renderer/Renderer.hpp"

#include "imgui.h"

namespace Hydrogen
{
	class Application
	{
	public:
		Application()
		{
			s_Instance = this;
		}
		~Application() = default;

		static Application* Get()
		{
			return s_Instance;
		}

		void OnResize(int width, int height);
		void Run();
		void ReloadShader();

		virtual void OnSetup() = 0;

		virtual void OnStartup() = 0;
		virtual void OnShutdown() = 0;
		virtual void OnUpdate() = 0;
		virtual void OnImGuiRender() = 0;

	public:
		struct ApplicationSpecification
		{
			std::string Name = "Hydrogen Application";
			glm::vec2 Version{ 1, 0 };

			std::string ViewportTitle = "Hydrogen Application";
			glm::vec2 ViewportSize{ 0 };
			glm::vec2 ViewportPos{ 0 };

			bool UseDebugGUI = false;
		};

		ApplicationSpecification ApplicationSpec;
		AssetManager MainAssetManager;
		std::shared_ptr<Viewport> MainViewport;
		std::shared_ptr<Scene> CurrentScene;

		std::shared_ptr<RenderContext> _RenderContext;
		std::shared_ptr<RenderPass> _RenderPass;
		std::shared_ptr<Pipeline> MainPipeline;

	private:
		static Application* s_Instance;
		ImVec2 m_ViewportSize;
	};
}
