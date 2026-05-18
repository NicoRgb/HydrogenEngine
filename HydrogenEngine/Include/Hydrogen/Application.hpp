#pragma once

#include <string>
#include <glm/glm.hpp>

#include "Hydrogen/Viewport.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/Renderer/DebugGUIRenderer.hpp"
#include "Hydrogen/AssetManager.hpp"

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
		void PhysicsUpdate(float deltaTime);
		void RenderImGui(std::shared_ptr<DebugGUIRenderer>& ImGuiRenderer);
		void SubmitImGui(std::shared_ptr<DebugGUIRenderer>& ImGuiRenderer);
		void ReloadShader();

		virtual void OnSetup() = 0;

		virtual void OnStartup() = 0;
		virtual void OnShutdown() = 0;
		virtual void OnUpdate(float deltaTime) = 0;
		virtual void OnImGuiRender() = 0;
		virtual void OnImGuiMenuBarRender() = 0;

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
		std::shared_ptr<SceneAsset> CurrentScene;
		std::shared_ptr<RenderContext> _RenderContext;

	private:
		const float timeStep = 1.0f / 60.0f;
		float accumulator = 0.0f;

		static Application* s_Instance;
	};
}
