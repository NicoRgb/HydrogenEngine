#pragma once

#include "Logger.hpp"

#ifdef HY_SYSTEM_WINDOWS
	#include <Windows.h>
	#define HY_ASSERT(x, ...) if (!(x)) { HY_ENGINE_FATAL(__VA_ARGS__); MessageBox(NULL, "An assertion failed. Look at the logs for more details", "Assert failure", MB_OK | MB_ICONEXCLAMATION); ExitProcess(EXIT_FAILURE); }
#else
	#define HY_ASSERT(x, ...) if (!(x)) { HY_ENGINE_FATAL(__VA_ARGS__); exit(1); }
#endif

namespace Hydrogen
{
	inline void HashCombine(size_t & seed, size_t value)
	{
		seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
	}
}
