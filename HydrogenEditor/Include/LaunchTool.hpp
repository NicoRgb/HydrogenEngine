#pragma once

#include <string>
#include <filesystem>

void LaunchTool(const std::string& path, const std::string& arguments, const std::string& workingDirectory);
std::filesystem::path GetCurrentExecutablePath();
