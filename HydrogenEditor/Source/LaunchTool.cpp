#include "LaunchTool.hpp"

#include <Hydrogen/Logger.hpp>

#ifdef _WIN32
#include <Windows.h>
#include <filesystem>

void LaunchTool(const std::string& path, const std::string& arguments, const std::string& workingDirectory)
{
	std::string commandLine = path + " " + arguments;

	HY_APP_INFO("Launching {}", path);

	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	if (CreateProcessA(
		NULL,
		&commandLine[0],
		NULL, NULL, FALSE,
		CREATE_NEW_CONSOLE,
		NULL, &workingDirectory[0],
		&si, &pi))
	{
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
	else
	{
		HY_APP_ERROR("Failed to launch process.");
	}
}
#endif
