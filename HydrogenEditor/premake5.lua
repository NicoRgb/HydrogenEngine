-- premake5.lua

project "HydrogenEditor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"

	targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir ("%{wks.location}/bin-int/" .. outputdir)

	includedirs { "%{wks.location}/HydrogenEngine/Include", "%{wks.location}/Extern/spdlog/include", "%{wks.location}/Extern/glm", "%{wks.location}/Extern/imgui-docking", "%{wks.location}/Extern/json/single_include/nlohmann", "Include" }
	links { "HydrogenEngine" }

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

	filter "action:vs*"
		buildoptions { "/utf-8" }
