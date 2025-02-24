#pragma once

#include <memory>
#include <string>
#include <glm/glm.hpp>

namespace Hydrogen
{
	class RenderContext
	{
	public:
		~RenderContext() = default;

		static std::shared_ptr<RenderContext> Create(std::string appName, glm::vec2 appVersion);
	};
}
