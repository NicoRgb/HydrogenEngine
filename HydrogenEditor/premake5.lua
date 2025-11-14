-- premake5.lua

project "HydrogenEditor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"

	targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir ("%{wks.location}/bin-int/" .. outputdir)

	includedirs { "%{wks.location}/HydrogenEngine/Include", "Include" }
	externalincludedirs { "%{wks.location}/Extern/spdlog/include", "%{wks.location}/Extern/glm", "%{wks.location}/Extern/imgui-docking", "%{wks.location}/Extern/ImTextEdit",
	"%{wks.location}/Extern/ImGuizmo", "%{wks.location}/Extern/json/single_include/nlohmann", "%{wks.location}/Extern/entt/single_include",
	"%{wks.location}/Extern/sol2/include", "%{wks.location}/Extern/bin/lua",
	"%{wks.location}/Extern/bin/reactphysics3d/include" }

	links { "HydrogenEngine" }

	files { "Include/**.h", "Include/**.hpp", "Source/**.cpp" }

	filter "%{wks.location}/Extern/**.h"
		warnings "Off"

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
