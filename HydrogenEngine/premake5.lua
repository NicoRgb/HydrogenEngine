-- premake5.lua

project "HydrogenEngine"
	kind "SharedLib"
	language "C++"
	cppdialect "C++17"

	targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir ("%{wks.location}/bin-int/" .. outputdir)

	includedirs { "Include" }

	defines { "HY_SYM_EXPORT" }

	files { "Include/**.h", "Include/**.hpp", "Source/**.cpp" }

	filter "configurations:Debug"
		defines { "HY_DEBUG" }
		optimize "Off"
		symbols "On"

	filter "configurations:Release"
		defines { "HY_NDEBUG", "NDEBUG" }
		optimize "On"
		symbols "On"

	filter "configurations:Dist"
		defines { "HY_DIST", "HY_NDEBUG", "NDEBUG" }
		optimize "On"
		symbols "Off"
