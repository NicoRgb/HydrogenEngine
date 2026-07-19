#include "LaunchTracy.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <filesystem>

void LaunchAndConnectTracy()
{
	auto path = std::filesystem::current_path() / "../Extern/bin/tracy/tracy-profiler.exe";
	ShellExecuteA(
		nullptr,
		"open",
		path.string().c_str(),
		"-a 127.0.0.1",
		nullptr,
		SW_SHOW
	);
}
#endif
