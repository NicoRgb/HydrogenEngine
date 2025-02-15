#pragma once

#ifdef HY_SYM_EXPORT
__declspec(dllexport) void Test();
#else
__declspec(dllimport) void Test();
#endif
