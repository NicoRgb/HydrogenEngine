#pragma once

#include <string>
#include <glm/glm.hpp>

#include "Hydrogen/Viewport.hpp"
#include "Hydrogen/AssetManager.hpp"
#include "Hydrogen/Scene.hpp"
#include "Hydrogen/AssetManager.hpp"

#include "Hydrogen/Renderer/RenderInstance.hpp"
#include "Hydrogen/Renderer/RenderDevice.hpp"
#include "Hydrogen/Renderer/SwapChain.hpp"
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
		~Application();

		static Application* Get()
		{
			return s_Instance;
		}

		void OnResize(int width, int height);
		void Run();
		void PhysicsUpdate(float deltaTime);
		const RenderDeviceDescriptor& GetCurrentRenderDeviceDesc() const;
		void ChangeRenderDevice(const RenderDeviceDescriptor& desc);
		const SwapChainSpec& GetCurrentSwapChainSepc() const { return m_CurrentSwapChainSpec; }
		void RecreateSwapchain(SwapChainSpec swapChainSepc);

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

	private:
		const float timeStep = 1.0f / 60.0f;
		float accumulator = 0.0f;

		std::unique_ptr<RenderInstance> m_RenderInstance;
		std::unique_ptr<RenderDevice> m_RenderDevice;
		std::unique_ptr<SwapChain> m_SwapChain;
		std::unique_ptr<Renderer> m_Renderer;

		SwapChainSpec m_CurrentSwapChainSpec;

		static Application* s_Instance;
	};
}
