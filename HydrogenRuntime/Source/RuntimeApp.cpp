#include <Hydrogen/Hydrogen.hpp>

class RuntimeApp : public Hydrogen::Application
{
public:
	virtual void OnSetup() override
	{
		ApplicationSpec.Name = "Hydrogen Runtime";
		ApplicationSpec.Version = { 1, 0 };
		ApplicationSpec.ViewportTitle = "Hydrogen Runtime";
		ApplicationSpec.ViewportSize = { 1080, 720 };
	}

	virtual void OnStartup() override
	{
	}

	virtual void OnShutdown() override
	{
	}

	virtual void OnUpdate() override
	{
	}
};

extern std::shared_ptr<Hydrogen::Application> GetApplication()
{
	return std::make_shared<RuntimeApp>();
}
