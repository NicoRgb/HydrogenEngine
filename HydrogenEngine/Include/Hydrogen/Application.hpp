#pragma once

#include <string>
#include <glm/glm.hpp>

#include "Core.hpp"

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
			std::string Name;
			glm::vec2 Version{ 0 };
		};

		ApplicationSpecification ApplicationSpec;
	};
}
