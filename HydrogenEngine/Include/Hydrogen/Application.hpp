#pragma once

#include <string>
#include <glm/glm.hpp>

#include "Hydrogen/Viewport.hpp"

namespace Hydrogen
{
	class Application
	{
	public:
		Application() = default;
		~Application() = default;

		void Run();

		virtual void OnSetup() = 0;

		virtual void OnStartup() = 0;
		virtual void OnShutdown() = 0;
		virtual void OnUpdate() = 0;

	protected:
		struct ApplicationSpecification
		{
			std::string Name = "Hydrogen Application";
			glm::vec2 Version{ 1, 0 };

			std::string ViewportTitle = "Hydrogen Application";
			glm::vec2 ViewportSize{ 0 };
			glm::vec2 ViewportPos{ 0 };
		};

		ApplicationSpecification ApplicationSpec;
		std::shared_ptr<Viewport> MainViewport;
	};
}
