-- premake5.lua

project "HydrogenEngine"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	characterset ( "MBCS" )

	targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir ("%{wks.location}/bin-int/" .. outputdir)

	includedirs { "Include", "%{wks.location}/Extern/spdlog/include", "%{wks.location}/Extern/glm" }

	files { "Include/**.h", "Include/**.hpp", "Source/**.cpp" }

	filter "system:windows"
		defines { "HY_SYSTEM_WINDOWS" }

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

	filter "action:vs*"
		buildoptions { "/utf-8" }
